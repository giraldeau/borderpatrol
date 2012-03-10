//
// $Id: trace.c,v 1.29 2009-04-19 03:28:02 ning Exp $
//
// library call wrappers
//

// #################################################################

#define _GNU_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <errno.h>

#include "config.h"

typedef enum {FALSE = 0, TRUE = 1} boolean;

int first_init = 0;
int sock;
int seq_no = 0;
char *prog_name;
char bin_name[1024];


struct sockaddr_in logd_addr;

void init();
void reinit();

#include "wrap.h"

#include "protocol.h"

// #################################################################

START_WRAP(int, pipe,(int filedes[2])) {
  int r = real_pipe(filedes);
  FD_SET(filedes[0], &tracked);
  FD_SET(filedes[1], &tracked);
  
  LOG("%d=pipe(%d,%d)", r,filedes[0],filedes[1]);
  return r;
  END_WRAP;
}

START_WRAP(int, dup,(int oldfd)) {
  int newfd = real_dup(oldfd);
  LOG("%d=dup(%d)", newfd, oldfd );
  if(newfd >= 0) FD_SET(newfd, &tracked);
  return newfd;
  END_WRAP;
}

START_WRAP(int, dup2,(int oldfd, int newfd)) {
  int r = real_dup2(oldfd,newfd);
  LOG("%d=dup2(%d,%d)", r, oldfd, newfd);
  if(r >= 0) FD_SET(r, &tracked);
  return r;
  END_WRAP;
}

int sock_types[MAX_FDS];
int sock_domains[MAX_FDS];
// TODO : bit map of 1=SOCK_STREAM 0=SOCK_DGRAM

START_WRAP(int, socket,(int domain, int type, int protocol)) {
  int s = real_socket(domain, type, protocol);
  LOG("%d=socket(%d,%d,%d)", s, domain, type, protocol);
  if (s >= 0) { // && domain == PF_INET && type == SOCK_STREAM) {
    FD_SET(s, &tracked);
    sock_types[s] = type;
    sock_domains[s] = domain;
  }
  return s;
  END_WRAP;
}

int sock_ports[MAX_FDS];
char *sock_files[MAX_FDS];
//int sock_rports[MAX_FDS];

START_WRAP(int, bind,(int s, const struct sockaddr *my_addr, socklen_t addrlen)) {
  if (!FD_ISSET(s, &tracked)) return real_bind(s, my_addr, addrlen);
  char portname[1024];
  int res = real_bind(s, my_addr, addrlen);
  if(my_addr->sa_family == AF_INET || my_addr->sa_family == AF_INET6) {
    snprintf(portname,1023,"%d", ntohs(((struct sockaddr_in *)my_addr)->sin_port));
    sock_ports[s] = ntohs(((struct sockaddr_in *)my_addr)->sin_port);
  }
  else if(my_addr->sa_family == AF_FILE) {
    //struct sockaddr_un *sa = (struct sockaddr_un *)addr;
    //strncpy(portname,((struct sockaddr_un *)addr)->sun_path,106);
    //portname[106] = '\0'; // just in case
    snprintf(portname,1023,"[file=%s]", ((struct sockaddr_un *)my_addr)->sun_path);
    //snprintf(portname,1023,"[file]");
    sock_files[s] = malloc(MAX_SOCK_FN);
    strncpy(sock_files[s],((struct sockaddr_un *)my_addr)->sun_path,1023);
  } else {
    snprintf(portname,1023, "[non-AF_INET,non-AF_FILE]");
  }

  LOG("bind(%d) to port %s", s, portname);
  return res;
  END_WRAP;
}


START_WRAP(int, listen,(int s, int backlog)) {
  if (!FD_ISSET(s, &tracked)) return real_listen(s, backlog);
  static int i = 0;
  i++;
  LOG("listen(%d,%d,%d)", s, backlog, i);
  return real_listen(s, backlog);
  END_WRAP;
}

START_WRAP(int, getsockname,(int s, struct sockaddr *name, socklen_t *namelen)) {
  int res;
  if (!FD_ISSET(s, &tracked))
    return real_getsockname(s, name, namelen);
  res = real_getsockname(s,name,namelen);
  LOG("%d=getsockname(%d)",res,s);
  return res;
  END_WRAP;
}

START_WRAP(int, accept,(int s, struct sockaddr *addr, socklen_t *addrlen)) {
  if (!FD_ISSET(s, &tracked)) return real_accept(s, addr, addrlen);
  int new_fd = real_accept(s, addr, addrlen);
  if(new_fd > 0 && sock_types[s] == SOCK_STREAM)
    pp_accept_dispatch(s,new_fd,sock_domains[s],sock_ports[s],sock_files[s]);

  /* carry on ... */

  // getpeername

  LOG("%d=accept(%d)", new_fd,s); // show the 4-tuple
  if(new_fd >= 0)
    FD_SET(new_fd, &tracked);
  return new_fd;
  END_WRAP;
}

START_WRAP(int, connect,(int s, const struct sockaddr* serv_addr, socklen_t addrlen)) {
  if (!FD_ISSET(s, &tracked)) return real_connect(s, serv_addr, addrlen);

  char portname[1024];
  if(serv_addr->sa_family == AF_INET) {
    snprintf(portname,1023,"%d", ntohs(((struct sockaddr_in *)serv_addr)->sin_port));
  } else if(serv_addr->sa_family == AF_FILE) {
    snprintf(portname,1023,"[file=%s]", ((struct sockaddr_un *)serv_addr)->sun_path);
  } else {
    snprintf(portname,1023, "[non-AF_INET,non-AF_FILE]");
  }
  LOG("connect_begin(%d) to %s", s, portname);
  int res = real_connect(s, serv_addr, addrlen);
  LOG("connect(%d) to %s", s, portname);
  return res;
  END_WRAP;
}

START_WRAP(int, close,(int s)) {
  char flag[128];
  if (!FD_ISSET(s, &tracked)) return real_close(s);
  FD_CLR(s, &tracked);
  flag[0] = '\0';
  int res = real_close(s);
  if(res == 0)
    pp_close_dispatch(s,flag,128);
  LOG("close(%d) [%s]", s, flag);
  return res;
  END_WRAP;
}

// OPEN
START_WRAP(int, open,(const char *pathname, int flags)) {
  //if (!FD_ISSET(s, &tracked)) return real_read(s, buf, len);
  //return real_open(pathname, flags);
  int res = real_open(pathname, flags);
  if(res>0)
    FD_SET(res, &tracked);
  LOG("%d=open(%s,%d)", (int)res, pathname, flags);
  return res;
  END_WRAP;
}

//START_WRAP(int, open,(const char *pathname, int flags, mode_t mode)) {
// int res = real_open(pathname, flags, mode);
//  LOG("%d=open(%s,%d)", (int)res, pathname, flags);
//  return res;
//  END_WRAP;
//}

START_WRAP(int, creat,(const char *pathname, mode_t mode)) {
  //if (!FD_ISSET(s, &tracked)) return real_read(s, buf, len);
  int res = real_creat(pathname, mode);
  LOG("%d=creat(%s)", (int)res, pathname);
  return res;
  END_WRAP;
}

// READING
char *read_cache;

START_WRAP(ssize_t, read,(int s, void *buf, size_t len)) {
  char flag[128];
  //if (!FD_ISSET(s, &tracked)) return real_read(s, buf, len); /// GOTCHA! psql read(4)

  pp_pipe_dispatch(s,bin_name);

  LOG("read_begin(%d,%d)", s, len);
  ssize_t res = real_read(s, buf, len);
  flag[0] = '\0';
  if(res>0)
    pp_read_dispatch(s,buf,res,flag,128);

  LOG("%d=read(%d,%d) [%s]", (int)res, s, len, flag);
  return res;
  END_WRAP;
}

START_WRAP(ssize_t, recv,(int s, void *buf, size_t len, int flags)) {
  char flag[128];
  if (!FD_ISSET(s, &tracked)) return real_recv(s, buf, len, flags);
  LOG("recv_begin(%d,%d,%d)", s, len, flags);
  ssize_t res = real_recv(s, buf, len, flags);
  flag[0] = '\0';
  if(res>0)
    pp_read_dispatch(s,buf,res,flag,128);
  LOG("recv(%d,%d,%d) [%s]", s, len, flags, flag);
  return res;
  END_WRAP;
}

START_WRAP(ssize_t, recvfrom,(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)) {
  if (!FD_ISSET(s, &tracked))
    return real_recvfrom(s, buf, len, flags, from, fromlen);
  LOG("recvfrom_begin(%d,%d,%d)", s, len, flags);
  ssize_t res = real_recvfrom(s, buf, len, flags, from, fromlen);
  LOG("recvfrom(%d,%d,%d)", s, len, flags);
  return res;
  END_WRAP;
}

START_WRAP(ssize_t, recvmsg,(int s, struct msghdr *msg, int flags)) {
  if (!FD_ISSET(s, &tracked)) return real_recvmsg(s, msg, flags);
  LOG("recvmsg(%d,%d)", s, flags);
  // TODO : _begin
  return real_recvmsg(s, msg, flags);
  END_WRAP;
}

// WRITING

START_WRAP(ssize_t, write,(int s, const void *buf, size_t len)) {
  char flag[128];
  //if (!FD_ISSET(s, &tracked)) return real_write(s, buf, len);
  LOG("write_begin(%d,%d)", s, len);
  ssize_t res = real_write(s, buf, len);
  flag[0] = '\0';
  if(res>0)
    pp_write_dispatch(s,buf,res,flag,128);
  LOG("%d=write(%d,%d) [%s]", res, s, len, flag);
  return res;
  END_WRAP;
}


START_WRAP(ssize_t, send,(int s, const void *buf, size_t len, int flags)) {
  if (!FD_ISSET(s, &tracked)) return real_send(s, buf, len, flags);
  LOG("send_begin(%d,%d,%d)", s, len, flags);
  ssize_t res = real_send(s, buf, len, flags);
  LOG("send(%d,%d,%d)", s, len, flags);
  return res;
  END_WRAP;
}

START_WRAP(ssize_t, sendto,(int  s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)) {
  if (!FD_ISSET(s, &tracked)) return real_sendto(s, buf, len, flags, to, tolen);
  LOG("sendto(%d,%d,%d)", s, len, flags);

  // TODO : _begin
  return real_sendto(s, buf, len, flags, to, tolen);
  END_WRAP;
}

START_WRAP(ssize_t, sendmsg,(int s, const struct msghdr *msg, int flags)) {
  if (!FD_ISSET(s, &tracked)) return real_sendmsg(s, msg, flags);
  LOG("sendmsg(%d,%d)", s, flags);

  return real_sendmsg(s, msg, flags);
  END_WRAP;
}

// MMAP
START_WRAP(void *, mmap,(void *start, size_t length, int prot , int flags, int fd, off_t offset)){
  LOG("mmap(%d,%d)", fd, (int)offset);
  return real_mmap(start,length,prot,flags,fd,offset);
  END_WRAP;
}

/* START_WRAP(void *, mmap2,(void *start, size_t length, int prot, int flags, int fd, off_t pgoffset)){ */
/*   LOG("mmap2(%d,%d)", fd, (int)pgoffset); */
/*   return real_mmap2(start,length,prot,flags,fd,pgoffset); */
/*   END_WRAP; */
/* } */

START_WRAP(int, munmap,(void *start, size_t length)){
  LOG("munmap()");
  return real_munmap(start,length);
  END_WRAP;
}

// SELECT
START_WRAP(int, select,(int  n, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout)) {
  LOG("select(%d)", n);

  return real_select(n, readfds, writefds, exceptfds, timeout);
  END_WRAP;
}

START_WRAP(int, pselect,(int n, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const struct timespec* timeout, const sigset_t* sigs)) {
  LOG("pselect(%d)", n);

  return real_pselect(n, readfds, writefds, exceptfds, timeout, sigs);
  END_WRAP;
}

// poll with serialization
struct pollfd* real_ufds;
nfds_t real_nfds;
int real_timeout;
int real_pollret = 0;
int seen_poll = 0;

//struct pollfd {
//  int fd;           /* file descriptor */
//  short events;     /* requested events */
//  short revents;    /* returned events */
//};

#define FOUT fp

START_WRAP(int, poll,(struct pollfd* ufds, nfds_t nfds, int timeout)) {
  int outret = 0; int k;
  LOG("poll_begin(nfds=%d,tm=%d)", nfds, timeout);
  // init
  if(!seen_poll) {
    seen_poll = 1;
    real_pollret = 0;
    real_ufds = malloc(sizeof(struct pollfd)*1000); // TODO : fix
    real_nfds = nfds;
    if(!real_ufds) {
      LOG("ERROR: can't malloc for poll");
      seen_poll = 0;
    }
  }

  // is there a cached event that we can reuse
  if(real_pollret > 0) {
    int i,j;
    for(i=0;i<nfds;i++) {
      for(j=0;j<real_nfds;j++) {
	// if there's an fd that the user is still interested in
	if (ufds[i].fd == real_ufds[j].fd
          //  ... that has a non-0 event
	    && real_ufds[j].revents) {
	  // then return it.
	  for(k=0;k<nfds;k++) ufds[k].revents = 0;
	  ufds[i].revents = real_ufds[j].revents;
	  real_ufds[j].revents = 0;
	  real_pollret -- ;
	  LOG("[virtual_cached]1=poll(nfds=%d,tm=%d) {fd=%d}", nfds, timeout, ufds[i].fd);
	  return 1;
	}
      }
    }
  }

  // nothing suitable was cached, so collect more events
  if(nfds) {
    // duplicate the ufds
    real_nfds = nfds;
    real_timeout = timeout;
    memcpy(real_ufds, ufds, sizeof(struct pollfd)*nfds);
    // call poll for real
    real_pollret = real_poll(real_ufds, real_nfds, real_timeout);
    LOG("[real]%d=poll(nfds=%d,tm=%d)", real_pollret, real_nfds, real_timeout);
    // TODO == -1?!!!!!!!!??????
  }

  // if we just got some, then virtualize
  if(real_pollret > 0) {
    int i;
    for(i=0;i<real_nfds;i++) {
      if(real_ufds[i].revents) {
	for(k=0;k<nfds;k++) ufds[k].revents = 0;
	ufds[i].revents = real_ufds[i].revents;
	real_ufds[i].revents = 0;
	real_pollret --;
	LOG("[virtual_repoll]1=poll(nfds=%d,tm=%d) {fd=%d}", nfds, timeout, ufds[i].fd);
	return 1;
      }
    }
  }

  // no events
  LOG("[virtual_trivial]0=poll(nfds=%d,tm=%d)", nfds, timeout);
  return 0;
  END_WRAP;
}

// epoll_create, epoll_ctl, epoll_wait


// TIMING
START_WRAP(int, gettimeofday,(struct timeval* tv, struct timezone* tz)) {
  // LOG("gettimeofday()");

  return real_gettimeofday(tv, tz);
  END_WRAP;
}


// FORK
START_WRAP(pid_t, fork,()) {
  int res;
  long ts;
  LOG("fork_begin()", res);
  res = real_fork();
  if(res == 0) {
    // strace the forked guy
    if(DEBUG_STRACE_KIDS && real_fork() == 0) {
      char fn[1024];
      char pd[1024];
      int r;
      sprintf(pd,"%d",getppid());
      sprintf(fn,"/tmp/strace.%d",getppid());
      LOG("stracing %d in [%s]", getppid(), fn);
      r = execlp("strace", "strace", "-q", "-p", pd, "-o", fn, 0);
      perror("strage");
      exit(1);
    }
    reinit();
    pp_init_log();
    LOG("fork() = %d",res);
  } else {
    LOG("fork() = %d",res);
  }
  return res;
  END_WRAP;
}

// #################################################################

// EXEC

char **hacked_envp;
START_WRAP(int, execve,(const char *filename, char *const argv [], char *const envp[])) {
  char *lib_path = real_getenv("LIBBTRACE_PATH");
  if (!lib_path) {
    lib_path = TRACE_LIB;
  }

  // tmp to get java working
  //LOG("execve(\"%s\")",filename);
  //return real_execve(filename,argv,hacked_envp);


  // sneak LD_PRELOAD back in
  setenv("LD_PRELOAD",lib_path,1);

  int i, last;
  char tmp[1024];
  for (i=0;envp[i]!=NULL;i++);
  last = i-1;
  hacked_envp = (char **)calloc(sizeof(char *), last+2);
  for(i=0;i<=last;i++)
    hacked_envp[i] = envp[i];
  sprintf(tmp,"LD_PRELOAD=%s",lib_path);
  hacked_envp[last] = tmp;
  hacked_envp[last+1] = NULL;

  // when you exec, want to know about stdin/stdout
  FD_SET(0, &tracked);
  FD_SET(1, &tracked);
  FD_SET(2, &tracked);

  int res = real_execve(filename,argv,hacked_envp);

  LOG("execve(\"%s\")",filename);
  strncpy(bin_name,filename,1024);
  real_unsetenv("LD_PRELOAD");
  return res;
  END_WRAP;
}

START_WRAP(int, execl,(const char *path, const char *arg, ...)) {

  LOG("execl(\"%s\")", path );
  return real_execl(path,arg);
  END_WRAP;
}

/* START_WRAP(int, execlp,(const char *file, const char *arg, ...)) { */
/*   LOG("execlp(\"%s\")",file);   */
/*   return real_execlp(file,arg); */
/*   END_WRAP; */
/* } */

// int execle(const char *path, const char *arg , ..., char * const envp[]);

/* START_WRAP(int, execle,(const char *path, const char *arg, ..., char * const envp[])) { */
/*   LOG("execle(\"%s\")",path);   */
/*   return real_execle(path,arg,envp); */
/*   END_WRAP; */
/* } */

/* START_WRAP(int, execv,(const char *path, char * const argv[])) { */
/*   LOG("execv(\"%s\")",path);   */
/*   return real_execv(path,argv); */
/*   END_WRAP; */
/* } */

/* START_WRAP(int, execvp,(const char *file, char * const argv[])) { */
/*   LOG("execvp(\"%s\")",file);   */
/*   return real_execvp(file,argv); */
/*   END_WRAP; */
/* } */

// #################################################################

START_WRAP(int, clearenv,()) {
  int r = real_clearenv();
  // tmp to get javaworking
  //setenv("LD_PRELOAD", TRACE_LIB, 1);
  LOG("clearenv()");
  return r;
  END_WRAP;
}

START_WRAP(int, unsetenv,(const char *name)) {
  // tmp to get javaworking
  //if(strncmp(name,"LD_PRELOAD",10)==0) return;
  real_unsetenv(name);
  //LOG("unsetenv(%s)",name);
  return;
  END_WRAP;
}

// #################################################################

//START_WRAP(void, exit,(int status)) {
void exit () __attribute__ ((noreturn));

void exit (int status) {
  init();
  LOG("exit(%d)",status);
  real_exit(status);
}

//START_WRAP(void, abort,()) {
void abort() __attribute__ ((noreturn));

void abort() {
  init();
  LOG("abort()");
  real_abort();
}

// #################################################################

static void pfdura_on(long pid) {
  int rc, fd;
  char tmp[4096];

  sprintf(tmp,"%s",RELAY_ON);
  fd = real_open(tmp, 02); //O_RDWR);
  if (fd < 0) {
    return;
  }

  sprintf(tmp, "%li", pid);
  rc = real_write(fd, tmp, strlen(tmp));
  real_close(fd);
  if (rc < 0) {
    printf("pfdura: write() error on %s: %s\n",
	   tmp, strerror(errno));
    //exit(1);
  }
}

// #################################################################

void init() {
  int pf;
  char buf[24];
  struct timeval tv;
  int pid = getpid();
  if(!first_init) {

    //grep START_WRAP trace.c | perl -n -e 'chomp; print if s/^START_WRAP\(([^,]+), ([^,]+),(\([^\)]*\))\) {/INIT_REAL($2); /;'
    INIT_REAL(pipe);
    INIT_REAL(dup);
    INIT_REAL(dup2);
    INIT_REAL(socket);
    INIT_REAL(bind);
    INIT_REAL(listen);
    INIT_REAL(accept);
    INIT_REAL(connect);
    INIT_REAL(close);
    INIT_REAL(getsockname);
    INIT_REAL(open);
    INIT_REAL(creat);
    INIT_REAL(read);
    INIT_REAL(recv);
    INIT_REAL(recvfrom);
    INIT_REAL(recvmsg);
    INIT_REAL(write);
    INIT_REAL(send);
    INIT_REAL(sendto);
    INIT_REAL(sendmsg);
    INIT_REAL(select);
    INIT_REAL(pselect);
    INIT_REAL(poll);
    INIT_REAL(gettimeofday);
    INIT_REAL(fork);
    INIT_REAL(execve);
    INIT_REAL(execl);
    INIT_REAL(clearenv);
    INIT_REAL(unsetenv); 

    INIT_REAL(mmap);
    INIT_REAL(munmap);
    //INIT_REAL(mmap2);

    INIT_REAL(exit);
    INIT_REAL(abort); 

    first_init = 1;
    seq_no = 0;
    /* other stuff */
    char *host_ip = real_getenv("LIBBTRACE_HOST");
    int host_port = LOGD_PORT;
    if (!host_ip) {
      host_ip = LOGD_IP;
    }
    if (real_getenv("LIBBTRACE_PORT")) {
      host_port = atoi(real_getenv("LIBBTRACE_PORT"));
    }
    prog_name = getenv("TRACE_FN");
    bin_name[0] = '\0';
    //fprintf(stderr,"--- first init pid=%d\n",pid); fflush(stderr);
    if((sock = real_socket(PF_INET, SOCK_STREAM, 0))<0) {
      fprintf(stderr,"couldn't create socket\n");fflush(stderr);
      exit;
    }
    /* Construct the server address structure */
    bzero(&logd_addr, sizeof(logd_addr));
    logd_addr.sin_family      = AF_INET;
    logd_addr.sin_addr.s_addr = inet_addr(host_ip);
    logd_addr.sin_port        = htons(host_port);
    /* Establish the connection to the echo server */
    if (real_connect(sock, (struct sockaddr *) &logd_addr,
		     sizeof(logd_addr)) < 0) {
      fprintf(stderr,"couldn't connect\n");
      fflush(stderr);
      exit;
    }
    LOG("START");
    //fprintf(stderr,"--- connected to logd on fd=%d.\n",sock);
    //fflush(stderr);

    // tell pfdura that we want tracking
    pfdura_on(getpid());
    LOG("enable_pfdura");

    /* */
    FD_SET(0, &tracked);
    FD_SET(1, &tracked);
    FD_SET(2, &tracked);
    FD_CLR(sock, &tracked);
  }
}

void reinit() {
  first_init = 0;
  init();
}
