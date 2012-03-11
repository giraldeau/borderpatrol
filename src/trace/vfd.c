//
// $Id: vfd.c,v 1.112 2009-04-19 03:28:02 ning Exp $
//
// virtualized fd library
//

#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include "config.h"
#include "protocol.h"
#include "../logd/binlog.h"
#include "vfd.h"
#include "wrap.h"
#include "cycles.h"

#define MIN(a,b) (a < b ? a : b)
#define MAX(a,b) (a > b ? a : b)

#define LOGD_FD 79

char                    first_init = 0;

extern char            *prog_name;

static fd_set           tracked;
static vfd_t            vfd_tab[MAX_FDS];
static vsel_t           vsel_data;

void vfd_init();
static void pfdura(int on, long pid);

int debug_events   = 0; // override with setenv DEBUG_EV=1
int bb_disable_pp  = 0; // override with setenv BB_DISABLE_PP=yes
int bb_disable_all = 0; // override with setenv BB_DISABLE_ALL=yes
extern int bb_disable_log; // from binlog

int pthread_mutex_lock_stub(pthread_mutex_t *m) {
  PROF_START(lock);
  int r = pthread_mutex_lock(m);
  PROF_END(lock);
  return r;
}

void pthread_mutex_unlock_stub(pthread_mutex_t *m) {
  PROF_START(unlock);
  pthread_mutex_unlock(m);
  PROF_END(unlock);
  return;
}

// ##########################################################
// Logging

//extern logentry_t   logbuf[LOGD_BUF_MAX];
//extern int          logbuf_free;
int mypid;
int logfd;

// ##########################################################
// exit/abort

// START_WRAP(void, exit,(int status))
// {
void exit() __attribute__ ((noreturn));

void exit(int status)
{
  if (!first_init)
    vfd_init("exit");
  pfdura(0, getpid());
  if(debug_events) LOGBIN0(SYS_EXIT,0);
  if(!bb_disable_all) lb_flush();
  real_exit(status);
}

// START_WRAP(void, abort,())
// {
void abort() __attribute__ ((noreturn));

void abort()
{
  if (!first_init)
    vfd_init("abort");
  //pfdura(0, getpid());
  //if(debug_events) LOGBIN0(SYS_ABORT,0);
  if(!bb_disable_all) lb_flush();
  real_abort();
}


// ##########################################################
// miscellaneous
//

// track/untrack page faults for pid
//
static void pfdura(int on, long pid)
{
  int   rc, fd;
  char  *file, tmp[32];

  file = (on ? RELAY_ON : RELAY_OFF);
  fd = real_open(file, O_RDWR);
  if (fd < 0) {
    return;
  }

  sprintf(tmp, "%li", pid);
  rc = real_write(fd, tmp, strlen(tmp));
  real_close(fd);
  if (rc < 0) {
    fprintf(stderr, "pfdura: write() error on %s: %s\n",
            file, strerror(errno)); fflush(stderr);
    real_exit(1);
  }
}

static void parse_sockaddr(const struct sockaddr *addr, int addrlen,
                           char *name, int len)
{
  if (addr->sa_family == AF_INET  ||  addr->sa_family == AF_INET6) {
    char host[INET_ADDRSTRLEN+1];
    host[0] = '\0';
    if (((struct sockaddr_in *)addr)->sin_addr.s_addr == INADDR_ANY)
      strcpy(host, "127.0.0.1");
    else
      inet_ntop(addr->sa_family,
                &((struct sockaddr_in *)addr)->sin_addr,
                host, INET_ADDRSTRLEN);
    int port = ntohs(((struct sockaddr_in *)addr)->sin_port);
    snprintf(name, len, "%s:%d", host, port);
  }
  else if (addr->sa_family == AF_FILE) {
    char file[MAX_SOCK_FN+1];
    int splen = addrlen - offsetof(struct sockaddr_un,
                                   sun_path);

    char *sp = ((struct sockaddr_un *)addr)->sun_path;
    if (sp[0] != '\0') {
      strncpy(file, sp, splen);
      file[splen] = '\0';
    }
    else {
      file[0] = 0;
      char tmp[16];
      int i;
      for (i = 0;  i < splen;  i++) {
        sprintf(tmp, "%2x", sp[i]);
        strcat(file, tmp);
      }
    }
    snprintf(name, len, "%s", file);
  }
  else
    strncpy(name, "unset", len);
}


// ##########################################################
// initialization
//

// grep START_WRAP vfd.c | perl -n -e 'chomp; print if s/^START_WRAP\(([^,]+), ([^,]+),(\([^\)]*\))\) {/INIT_REAL($2); /;'

// set up real_*()
//
void wrap_init()
{
  PROF_START(wrap_init);
  INIT_REAL(exit);
  INIT_REAL(abort); 
  INIT_REAL(pipe);
  INIT_REAL(dup);
  INIT_REAL(dup2);
  INIT_REAL(fcntl);
  INIT_REAL(socket);
  INIT_REAL(bind);
  INIT_REAL(listen);
  INIT_REAL(accept);
  INIT_REAL(connect);
  INIT_REAL(close);
  // INIT_REAL(getsockname);
  // INIT_REAL(getpeername);
  INIT_REAL(open);
  INIT_REAL(creat);
  INIT_REAL(read);
  INIT_REAL(readv);
  INIT_REAL(recv);
  INIT_REAL(recvfrom);
  // INIT_REAL(recvmsg);
  INIT_REAL(write);
  INIT_REAL(writev);
  INIT_REAL(send);
  INIT_REAL(sendto);
  INIT_REAL(sendfile);
  INIT_REAL(sendfile64);
  // INIT_REAL(sendmsg);
  INIT_REAL(select);
  // INIT_REAL(pselect);
  INIT_REAL(poll);
  // INIT_REAL(gettimeofday);
  INIT_REAL(fork);
  INIT_REAL(execve);
  INIT_REAL(execl);
  INIT_REAL(execle);
  INIT_REAL(execv);
  INIT_REAL(execlp);
  INIT_REAL(execvp);
  INIT_REAL(getenv);
  INIT_REAL(clearenv);
  INIT_REAL(unsetenv); 
  // INIT_REAL(mmap);
  // INIT_REAL(munmap);
  // INIT_REAL(mmap2);
  INIT_REAL(pthread_create);
  //INIT_REAL(pthread_atfork);
  PROF_END(wrap_init);
}

// connect to logd
//

void logd_init()
{
  PROF_START(logd_init);
  int newfd;
  if(bb_disable_log || bb_disable_all) {
    //fprintf(stderr,"warning!!!!!!!!!!!!!!!\n");
    logfd = 79; goto logd_init_done;
  }

#ifdef LOGD_USE_SOCK
  if ((logfd = real_socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr,"couldn't create socket\n"); fflush(stderr);
    real_exit(1);
  }
  struct sockaddr_in logd_addr;
  char *host_ip = real_getenv("LIBBTRACE_HOST");
  int host_port = LOGD_PORT;
  if (!host_ip) {
    host_ip = LOGD_IP;
  }
  if (real_getenv("LIBBTRACE_PORT")) {
    host_port = atoi(real_getenv("LIBBTRACE_PORT"));
  }
  memset(&logd_addr, 0, sizeof(logd_addr));
  logd_addr.sin_family      = AF_INET;
  logd_addr.sin_addr.s_addr = inet_addr(host_ip);
  logd_addr.sin_port        = htons(host_port);
  if (real_connect(logfd, (struct sockaddr *)&logd_addr,
                   sizeof(logd_addr)) < 0) {
    fprintf(stderr,"couldn't connect to logd\n"); fflush(stderr);
    real_exit(1);
  }
#elif LOGD_PIPE_PER_PID
  if( (logfd=real_open(LOGD_PIPE, O_WRONLY)) < 0 ) {
    perror("");
    fprintf(stderr,"couldn't connect to named pipe: %s\n", LOGD_PIPE);
    fflush(stderr); real_exit(1);
  }
#else // LOGD_PIPE_PER_TID
  if( (logfd=real_open(LOGD_PIPE, O_WRONLY)) < 0 ) {
    perror("");
    fprintf(stderr,"couldn't connect to named pipe: %s\n", LOGD_PIPE);
    fflush(stderr); real_exit(1);
  }
#endif

  // don't use fd3 because other programs (like thttpd) have conventions
  newfd = real_dup2(logfd,LOGD_FD); real_close(logfd); logfd = newfd;

 logd_init_done:
  FD_CLR(logfd, &tracked);
  alloc_ctx(mypid,logfd);
  PROF_END(logd_init);
}

void vfd_tab_init_one(vfd_t *v) {
  //if (v->pp_dispatcher)
  //v->pp_dispatcher->pp_shutdown(v->pp_data);
  v->pp_dispatcher = NULL;
  v->type = 0;
  v->rbuflen = v->rbufofs = v->rlastlen = v->tosend = v->ppwantmore = 0;
  v->wbuflen = v->wbufofs = 0;
  v->in_prog = 0;
  //pthread_mutex_init(&(v->lock), NULL);
}

// reset tracked fds, fd table and select cache
//
void vfd_tab_init()
{
  //if(bb_disable_all) return;

  PROF_START(vfd_tab_init);
  //fprintf(stderr,"vfd_tab_init()\n"); fflush(stderr);
  FD_ZERO(&tracked);
  FD_SET(0, &tracked);
  FD_SET(1, &tracked);
  FD_SET(2, &tracked);

  //BAD: for (i = 0;  i < MAX_FDS;  i++) {
  //BAD: memset(&vfd_tab[i], 0, sizeof(vfd_t));
  //pthread_mutex_init(v->lock, NULL);

  // init select() cache
  memset(&vsel_data, 0, sizeof(vsel_t));
  FD_ZERO(&vsel_data.read);
  FD_ZERO(&vsel_data.write);
  FD_ZERO(&vsel_data.except);
  pthread_mutex_init(&vsel_data.lock, NULL);
  PROF_END(vfd_tab_init);
}

// forked process limited init
//
void fork_init()
{
  PROF_START(fork_init);
  //fprintf(stderr,"fork_init()\n"); fflush(stderr);
  mypid = getpid();
  pfdura(1, mypid);
  // now that we have named pipes, only call this on global init:
  // logd_init(); 
  // need to reinit so get_pid() works
  alloc_ctx(mypid,logfd);
  PROF_END(fork_init);
}

// global shutdown
void vfd_shutdown(void) {
  if(bb_disable_all) return;
  PROF_START(vfd_shutdown);
  LOGBIN0(SYS_SHUTDOWN,0);
  lb_flush();
  real_close(logfd);
  PROF_END(vfd_shutdown);
}

// global init
//
void vfd_init(char *caller)
{
  char *ept,*path,*s;
  INIT_REAL(getenv);

  // see if we're in slim mode
  if(real_getenv("BB_DISABLE_ALL"))
    bb_disable_all = 1;

  //fprintf(stderr,"vfd_init()\n"); fflush(stderr);
  PROF_START(vfd_init_0);
  mypid = getpid();
  atexit(vfd_shutdown);
  PROF_END(vfd_init_0);


  PROF_START(vfd_init_a);
  wrap_init();
  logd_init();
  fork_init();
  vfd_tab_init();
  first_init = 1;
  PROF_END(vfd_init_a);
  PROF_START(vfd_init_b);

  // collect prog_name
  prog_name = real_getenv("TRACE_FN");
  if(prog_name == NULL) prog_name = "__";

  // collect DEBUG mode
  if(real_getenv("DEBUG_EVENTS") ||
     real_getenv("DEBUG_EV"))
    debug_events = 1;

  // collect BB_DISABLE_PP DEBUG MODE
  if(real_getenv("BB_DISABLE_PP"))
    bb_disable_pp = 1;

  // if we came from an exec(), check the env
  //   to collect the parent's tid
  INIT_REAL(getenv);
  ept = real_getenv("BB_EXEC_PTID");
  if(ept != NULL) {
    unsigned long long int ptid = 666;
    ptid = strtoull(ept, NULL, 10);
    LOGBINli(SYS_EXEC_CHILD,0,ptid,mypid);
    real_unsetenv("BB_EXEC_PTID");
  }

  path = real_getenv("BB_EXEC_PATH");
  if(path != NULL) {
    vfd_t *v = &vfd_tab[0];
    v->pp_dispatcher = pp_id_exec(path);
    //if(v->pp_dispatcher)
    //v->pp_dispatcher->pp_init(&v->pp_data); // happens on accept()
    real_unsetenv("BB_EXEC_PATH");
  }  

  PROF_END(vfd_init_b);
}

// ##########################################################
// virtualized write
// Keeps calling PP until PP returns -1, indicating it needs
// more data.

void vwrite_req(vfd_t *v, int s, const void *buf, int len) {
  PROF_START(vwrite_req);
  int vlen = 0,req;

  if (!v->pp_dispatcher) return;
  if (len <= 0) return;

  // append the amount we wrote to the virtual buffer
  memcpy(&v->wbuf[v->wbuflen], buf, len);
  v->wbuflen += len;
  do {
    vlen = v->wbuflen - v->wbufofs;
    if (vlen == 0)
      break;

    req = v->pp_dispatcher->pp_write(
	 v->pp_data, s, &(v->wbuf[v->wbufofs]), vlen);

    // either we gave you some data which you used,
    //   or you returned -1 because you want more
    // assert(req != 0);

    // got a chunk. call me again
    if(req > 0)
      v->wbufofs += req;

  } while(req > 0);

  // save what was unused for next time application
  // calls write()
  memmove(&(v->wbuf[0]),&(v->wbuf[v->wbufofs]), vlen);
  v->wbufofs = 0;
  v->wbuflen = vlen;

  // don't need to clear the vsel 'write' flag, because
  // caller will do it.

  PROF_END(vwrite_req);
  return;
}

// ##########################################################
// wrappers
//

START_WRAP(int, pipe,(int filedes[2]))
{
  if(bb_disable_all) return real_pipe(filedes);
  int r = real_pipe(filedes);

  if (r == 0) {
    FD_SET(filedes[0], &tracked);
    FD_SET(filedes[1], &tracked);
    vfd_t *v0 = &vfd_tab[filedes[0]],
          *v1 = &vfd_tab[filedes[1]];
    vfd_tab_init_one(v0);
    vfd_tab_init_one(v1);
    v0->type = v1->type = VFD_PIPE;
  }

  LOGBINii(SYS_PIPE, r, filedes[0], filedes[1]);
  return r;
  END_WRAP;
}

void dup_vfd(int oldfd, int newfd) {
  vfd_tab[newfd] = vfd_tab[oldfd]; // same dispatcher, data, etc

  vfd_t *v = &vfd_tab[newfd];
  vfd_tab_init_one(v);
  v->type  = VFD_DUP;              // ... but remember it was dup'd
  v->in_prog = 0;
}

START_WRAP(int, dup,(int oldfd))
{
  if(bb_disable_all) return real_dup(oldfd);
  int newfd = real_dup(oldfd);

  if (oldfd < 0  ||  oldfd >= MAX_FDS)
    //      ||  !FD_ISSET(oldfd, &tracked))
    return newfd;

  if (newfd >= 0) {
    FD_SET(newfd, &tracked);
    dup_vfd(oldfd,newfd);
  }

  LOGBINi(SYS_DUP,newfd,oldfd);
  return newfd;
  END_WRAP;
}

START_WRAP(int, dup2,(int oldfd, int newfd))
{
  if(bb_disable_all) return real_dup2(oldfd, newfd);
  //if(newfd == LOGD_FD) return -1;

  if (oldfd < 0  ||  oldfd >= MAX_FDS
      //      ||  !FD_ISSET(oldfd, &tracked)
      ||  newfd < 0  ||  newfd >= MAX_FDS)
    return real_dup2(oldfd, newfd);

  int r = real_dup2(oldfd, newfd);

  if (r >= 0) {
    FD_SET(r, &tracked);
    dup_vfd(oldfd,r);
  }

  LOGBINi(SYS_DUP,r,oldfd);
  return r;
  END_WRAP;
}

START_WRAP(int, fcntl,(int fd, int cmd, ...))
{
  va_list cp;
  va_start(cp,cmd);
  long  r_arg = va_arg(cp, long);

  int r = real_fcntl(fd,cmd,r_arg);
  if(bb_disable_all) return r;
  if(cmd == F_DUPFD) {

    if (r >= 0) {
      FD_SET(r, &tracked);
      dup_vfd(fd,r);
    }
    if(!bb_disable_all) LOGBINi(SYS_DUP_FCNTL,r,fd);
  }

  va_end(cp);
  return r;
  END_WRAP;
}

START_WRAP(int, socket,(int domain, int type, int protocol))
{
  if(bb_disable_all) return real_socket(domain, type, protocol);
  int s = real_socket(domain, type, protocol);

  if (s >= 0) {
    FD_SET(s, &tracked);
    vfd_t *v = &vfd_tab[s];
    vfd_tab_init_one(v);
    v->type = VFD_SOCK;
    v->in_prog = 0;
  }

  LOGBINiii(SYS_SOCKET,s,domain, type, protocol);
  
  return s;
  END_WRAP;
}

START_WRAP(int, bind,(int s, const struct sockaddr *my_addr, socklen_t addrlen))
{
  if(bb_disable_all) return real_bind(s, my_addr, addrlen);
  int r = real_bind(s, my_addr, addrlen);

  if (s < 0  ||  s >= MAX_FDS) // ||  !FD_ISSET(s, &tracked))
    return r;

  if (r == 0) {
    vfd_t *v = &vfd_tab[s];
    v->pp_dispatcher = pp_id_bind(my_addr);
  }

  if(debug_events) LOGBINi(SYS_BIND,r,s);

  return r;
  END_WRAP;
}

START_WRAP(int, listen,(int s, int backlog))
{
  if(bb_disable_all) return real_listen(s, backlog);
  int r = real_listen(s, backlog);

  if (s < 0  ||  s >= MAX_FDS)//  ||  !FD_ISSET(s, &tracked))
    return r;

  if(debug_events) LOGBINii(SYS_LISTEN,r,s, backlog);
  return r;
  END_WRAP;
}

START_WRAP(int, accept,(int s, struct sockaddr *addr, socklen_t *addrlen))
{
  if(bb_disable_all) return real_accept(s, addr, addrlen);
  char ctmp[MAX_LOG_LINE];
  struct sockaddr laddr;
  struct sockaddr raddr;
  if (s < 0  ||  s >= MAX_FDS)//  ||  !FD_ISSET(s, &tracked))
    return real_accept(s, addr, addrlen);

  LOGBINi(SYS_ACCEPT_BEGIN,0,s);
  int new_fd = real_accept(s, addr, addrlen);

  if (pthread_mutex_lock_stub(&vsel_data.lock) != 0)
    LOGBINfatal("accept");
  else {
    FD_CLR(s, &vsel_data.read);
    pthread_mutex_unlock_stub(&vsel_data.lock);
  }

  char lhostport[512], rhostport[512];
  strcpy(lhostport, "other");
  strcpy(rhostport, "other");

  if (new_fd != -1) {
    FD_SET(new_fd, &tracked);
    vfd_t *v = &vfd_tab[new_fd];
    vfd_tab_init_one(v);
    v->type = VFD_SOCK;
    v->in_prog = 0;

    unsigned int raddrlen = sizeof (struct sockaddr);
    if (getpeername(new_fd, &raddr, &raddrlen) != -1)
      parse_sockaddr(&raddr, raddrlen, rhostport, 512);

    unsigned int laddrlen = sizeof (struct sockaddr);
    if (getsockname(new_fd, &laddr, &laddrlen) != -1)
      parse_sockaddr(&laddr, laddrlen, lhostport, 512);
    
    // We should call id_bind here instead because
    // i've seen cases where the socket is bind()'d to
    // port zero, but here accepts on port 9090

    //vfd_t *vs = &vfd_tab[s];
    //v->pp_dispatcher = vs->pp_dispatcher;
    //if (v->pp_dispatcher)
    //v->pp_dispatcher->pp_init(new_fd,&v->pp_data);

    v->pp_dispatcher = pp_id_bind(&laddr);
    if(v->pp_dispatcher)
      v->pp_dispatcher->pp_init(new_fd,&v->pp_data);
  }

  strcpy(ctmp,rhostport);
  strcat(ctmp,"->");
  strcat(ctmp,lhostport);
  LOGBINis(SYS_ACCEPT,new_fd,s,ctmp);
  //LOGBINiaa(SYS_ACCEPT,new_fd,s,&laddr,&raddr);

  return new_fd;
  END_WRAP;
}

void connect_event(int s, int r) {

  char ctmp[MAX_LOG_LINE+1];
  struct sockaddr_storage laddr, raddr;
  char lhostport[1024], rhostport[1024];
  unsigned int laddrlen = sizeof (laddr);
  unsigned int raddrlen = sizeof (raddr);

  strcpy(lhostport, "other");
  strcpy(rhostport, "other");

  if (getpeername(s, (struct sockaddr *)&raddr, &raddrlen) != -1)
    parse_sockaddr((struct sockaddr *)&raddr, raddrlen, rhostport, 1024);

  if (getsockname(s, (struct sockaddr *)&laddr, &laddrlen) != -1)
    parse_sockaddr((struct sockaddr *)&laddr, laddrlen, lhostport, 1024);

  vfd_t *v = &vfd_tab[s];
  v->in_prog = 0;
  v->pp_dispatcher = pp_id_connect(&raddr);
  if (v->pp_dispatcher)
    v->pp_dispatcher->pp_init(s,&v->pp_data);

  strcpy(ctmp,lhostport);
  strcat(ctmp,"->");
  strcat(ctmp,rhostport);
  LOGBINis(SYS_CONNECT,r,s,ctmp);

  return;
}

START_WRAP(int, connect,(int s, const struct sockaddr* serv_addr, socklen_t addrlen))
{
  if(bb_disable_all) return real_connect(s, serv_addr, addrlen);
  if (s < 0  ||  s >= MAX_FDS)//  ||  !FD_ISSET(s, &tracked))
    return real_connect(s, serv_addr, addrlen);

  LOGBINi(SYS_CONNECT_BEGIN,0,s);
  int r = real_connect(s, serv_addr, addrlen);

  // connect succeeded now
  if (r != -1)
    connect_event(s, r);

  // connect event will happen in the future (non-block sock)
  else if (errno == EINPROGRESS) {
    LOGBINs(SYS_DEBUG,r,"connect got EINPROGRESS");
    vfd_t *v = &vfd_tab[s];
    v->in_prog = 1;
  }
  
  // just plain failed
  else
    LOGBINis(SYS_CONNECT,r,s,"failed->failed");

  return r;
  END_WRAP;
}

START_WRAP(int, close,(int s))
{
  if(s == LOGD_FD) { return 1; }
  if(bb_disable_all) return real_close(s);

  if (s < 0  ||  s >= MAX_FDS)//  ||  !FD_ISSET(s, &tracked))
    return real_close(s);

  FD_CLR(s, &tracked);
  vfd_t *v = &vfd_tab[s];
  v->type = VFD_NONE;
  v->in_prog = 0;
  v->rbufofs = v->rbuflen = 0;
  if (v->pp_dispatcher) {
    v->pp_dispatcher->pp_shutdown(&v->pp_data);
    v->pp_dispatcher = NULL;
  }

  if (pthread_mutex_lock_stub(&vsel_data.lock) != 0)
    LOGBINfatal("close");
  else {
    FD_CLR(s, &vsel_data.read);
    FD_CLR(s, &vsel_data.write);
    FD_CLR(s, &vsel_data.except);
    pthread_mutex_unlock_stub(&vsel_data.lock);
  }

  int r = real_close(s);

  LOGBINi(SYS_CLOSE,r,s);
  return r;
  END_WRAP;
}

// START_WRAP(int, getsockname,(int s, struct sockaddr *name,
//                              socklen_t *namelen))
// {
//   int r = real_getsockname(s, name, namelen);
// 
//   if (s < 0  ||  s >= MAX_FDS  ||  !FD_ISSET(s, &tracked))
//     return r;
// 
//   LOGSPRINTF("%d=getsockname(%d),err=%s", r, s, r == -1 ? strerror(errno) : "0");
//   return r;
//   END_WRAP;
// }

// START_WRAP(int, getpeername,(int s, struct sockaddr *name,
//                              socklen_t *namelen))
// {
//   int r = real_getpeername(s, name, namelen);
// 
//   if (s < 0  ||  s >= MAX_FDS  ||  !FD_ISSET(s, &tracked))
//     return r;
// 
//   LOGSPRINTF("%d=getpeername(%d),err=%s", r, s, r == -1 ? strerror(errno) : "0");
//   return r;
//   END_WRAP;
// }

//int open(const char *pathname, int flags);
//int open(const char *pathname, int flags, mode_t mode);

START_WRAP(int, open,(const char *pathname, int flags, ...))
{
  int r = real_open(pathname, flags);
  if(bb_disable_all) return r;

  if (r != -1) {
    FD_SET(r, &tracked);
    vfd_t *v = &vfd_tab[r];
    vfd_tab_init_one(v);
    v->type = VFD_FILE;
    v->in_prog = 0;
  }

  LOGBINis(SYS_OPEN,r,flags,pathname);
  return r;
  END_WRAP;
}

// TODO: varargs?
// START_WRAP(int, open,(const char *pathname, int flags, mode_t mode))
// {
//   int r = real_open(pathname, flags, mode);
//   LOGSPRINTF("%d=open(%s,%d)", r, pathname, flags);
//   return r;
//   END_WRAP;
// }

START_WRAP(int, creat,(const char *pathname, mode_t mode))
{
  if(bb_disable_all) return real_creat(pathname, mode);
  int r = real_creat(pathname, mode);

  if (r != -1) {
    FD_SET(r, &tracked);
    vfd_t *v = &vfd_tab[r];
    vfd_tab_init_one(v);
    v->type = VFD_FILE;
    v->in_prog = 0;
  }

  LOGBINis(SYS_CREATE,r,0,pathname);
  return r;
  END_WRAP;
}

// ##########################################################
// ### reading                                            ###
// ##########################################################

void clear_readable_cache(int s){
  if (pthread_mutex_lock_stub(&vsel_data.lock) != 0)
    LOGBINfatal("read");
  else {
    FD_CLR(s, &vsel_data.read);
    pthread_mutex_unlock_stub(&vsel_data.lock);
  }
}
void set_readable_cache(int s){
  if (pthread_mutex_lock_stub(&vsel_data.lock) != 0)
    LOGBINfatal("read");
  else {
    FD_SET(s, &vsel_data.read);
    pthread_mutex_unlock_stub(&vsel_data.lock);
  }
}

void show_buffer(int sc,vfd_t *v) {
  if(!debug_events) return;
  //  int l = (v->rbuflen) - (v->rbufofs);
  if(v->rbuflen > 0) {
    //char *tmp = malloc(v->rbuflen+1);
    //strncpy(tmp,v->rbuf,v->rbuflen);
    //tmp[v->rlastlen] = '%';
    //tmp[v->rbufofs] = '#';
    LOGBINiiis(sc,0,v->rbufofs,v->rbuflen,v->tosend,"<occu>");
    //free(tmp);
  } else {
    LOGBINiiis(sc,0,v->rbufofs,v->rbuflen,v->tosend,"<empty>");
  }
  lb_flush();
}

// shift the entire buffer (and all pointers) to the left
void buf_shift(vfd_t *v) {
  int begin = MIN(v->rbufofs,v->rlastlen);
  if (begin == 0) return;

  int tomove = v->rbuflen - begin;
  memmove(&v->rbuf[0],&v->rbuf[begin],tomove);
  v->rbufofs -= begin;
  v->rbuflen -= begin;
  v->rlastlen -= begin;
  show_buffer(SYS_BUF_MOVED,v);
}

// return a piece of the buffer
ssize_t buf_ret(vfd_t *v,int s,char *out,size_t out_requested) {
  assert(v); assert(v->tosend);
  assert(v->rlastlen < VBUFSIZE);
  size_t out_cando = (size_t)MIN((int)out_requested,v->tosend);
  memcpy(out,&v->rbuf[v->rlastlen],out_cando);
  v->rlastlen += (int)out_cando;
  v->tosend   -= (int)out_cando;
  assert(v->tosend >= 0);
  // the final situation here: next time
  //  we get called, we will real_read for sure!
  if(v->rlastlen == v->rbuflen)
    clear_readable_cache(s);
  // otherwise, assume there's more cached data
  else 
    set_readable_cache(s);
  show_buffer(SYS_BUF_RETURNED,v);
  return (ssize_t)out_cando;
}

void present_pp(vfd_t *v,int s) {
  int amt = v->rbuflen - v->rbufofs;
  if(amt == 0) return;
  v->ppwantmore = 0;
  int a = v->pp_dispatcher->pp_read(v->pp_data,s, &v->rbuf[v->rbufofs], amt);

  if(a < 0) {
    v->tosend = v->rbuflen - v->rlastlen;
    v->ppwantmore = 1;
    
  } else {
    if(a < amt)
      LOGBINii(SYS_PP_ISO_READ,0,a,amt);
    v->rbufofs += a;
    v->tosend = v->rbufofs - v->rlastlen;
  }

  show_buffer(SYS_BUF_PPOFS,v);
}

// ##########################################################

ssize_t virtual_read(vfd_t *v, int s, void *buf, size_t len) {
  ssize_t r;
  show_buffer(SYS_BUF_PRE,v);
  // short loop: keep coming back
  if(v->tosend) {
    if(debug_events) LOGBINiii(SYS_BUF_SHORT1,0,v->rbufofs,v->rbuflen,v->tosend);
    return buf_ret(v,s,buf,len);
  }

  // app caught up to PP. see if PP can work with more data
  if(v->rbufofs < v->rbuflen && !v->ppwantmore) {
    present_pp(v,s);
    if(v->tosend) {
      if(debug_events) LOGBINiii(SYS_BUF_SHORT2,0,v->rbufofs,v->rbuflen,v->tosend);
      return buf_ret(v,s,buf,len);
    }
  }

  // here, nothing cached for app and PP needs more to continue,
  // so we must call real_read()
  assert(v->tosend == 0);
  buf_shift(v);
  r = real_read(s,&v->rbuf[v->rbuflen],VBUFSIZE - v->rbuflen);
  if(r<=0) return r;

  // having just read, present new data to PP
  v->rbuflen += r;
  show_buffer(SYS_BUF_REFILLED,v);
  present_pp(v,s);
  assert(v->tosend);
  return buf_ret(v,s,buf,len);
}

START_WRAP(ssize_t, read,(int s, void *buf, size_t len))
{
  if(bb_disable_all) return real_read(s, buf, len);
  if (s < 0  ||  s >= MAX_FDS)
    return real_read(s, buf, len);

  ssize_t r;
  vfd_t *v = &vfd_tab[s];
  LOGBINii(SYS_READ_BEGIN,0,s, len);

  if(v->pp_dispatcher) {
    r = virtual_read(v,s,buf,len);
    assert(v->rbufofs <= v->rbuflen);
  } else {
    r = real_read(s,buf,len);
    clear_readable_cache(s);
  }

  LOGBINii(SYS_READ,r,s,len);
  return r;
  END_WRAP;
}
  
START_WRAP(ssize_t, readv,(int fd, const struct iovec *vector, int count))
{
  if(bb_disable_all) return real_readv(fd, vector, count);
  if (fd < 0  ||  fd >= MAX_FDS)
    return real_readv(fd, vector, count);

  return read(fd,vector[0].iov_base, vector[0].iov_len);
  END_WRAP;
}

// ##########################################################

ssize_t virtual_recvfrom(vfd_t *v, int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen) {
  ssize_t r;
  show_buffer(SYS_BUF_PRE,v);
  // short loop: keep coming back
  if(v->tosend) {
    if(debug_events) LOGBINiii(SYS_BUF_SHORT1,0,v->rbufofs,v->rbuflen,v->tosend);
    return buf_ret(v,s,buf,len);
  }

  // app caught up to PP. see if PP can work with more data
  if(v->rbufofs < v->rbuflen && !v->ppwantmore) {
    present_pp(v,s);
    if(v->tosend) {
      if(debug_events) LOGBINiii(SYS_BUF_SHORT2,0,v->rbufofs,v->rbuflen,v->tosend);
      return buf_ret(v,s,buf,len);
    }
  }

  // here, nothing cached for app and PP needs more to continue,
  // so we must call real_read()
  assert(v->tosend == 0);
  buf_shift(v);
  r = real_recvfrom(s,&v->rbuf[v->rbuflen],VBUFSIZE - v->rbuflen, flags,from,fromlen);
  if(r<=0) return r;

  // having just read, present new data to PP
  v->rbuflen += r;
  show_buffer(SYS_BUF_REFILLED,v);
  present_pp(v,s);
  //assert(v->tosend);
  return buf_ret(v,s,buf,len);
}

START_WRAP(ssize_t, recvfrom,(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen))
{
  if(bb_disable_all) return real_recvfrom(s, buf, len, flags, from, fromlen);
  if (s < 0  ||  s >= MAX_FDS)
    return real_recvfrom(s, buf, len, flags, from, fromlen);

  ssize_t r;
  vfd_t *v = &vfd_tab[s];

  LOGBINiii(SYS_RECVFROM_BEGIN,0,s, len, flags);
  if(v->pp_dispatcher) {
    r = virtual_recvfrom(v,s,buf,len,flags,from,fromlen);
    assert(v->rbufofs <= v->rbuflen);
  } else {
    r = real_recvfrom(s,buf,len,flags,from,fromlen);
    clear_readable_cache(s);
  }
  LOGBINiii(SYS_RECVFROM,r,s, len, flags);

  return r;
  END_WRAP;
}

// ##########################################################

ssize_t virtual_recv(vfd_t *v, int s, void *buf, size_t len, int flags) {
  ssize_t r;
  show_buffer(SYS_BUF_PRE,v);
  // short loop: keep coming back
  if(v->tosend) {
    if(debug_events) LOGBINiii(SYS_BUF_SHORT1,0,v->rbufofs,v->rbuflen,v->tosend);
    return buf_ret(v,s,buf,len);
  }

  // app caught up to PP. see if PP can work with more data
  if(v->rbufofs < v->rbuflen && !v->ppwantmore) {
    present_pp(v,s);
    if(v->tosend) {
      if(debug_events) LOGBINiii(SYS_BUF_SHORT2,0,v->rbufofs,v->rbuflen,v->tosend);
      return buf_ret(v,s,buf,len);
    }
  }

  // here, nothing cached for app and PP needs more to continue,
  // so we must call real_read()
  assert(v->tosend == 0);
  buf_shift(v);
  r = real_recv(s,&v->rbuf[v->rbuflen],VBUFSIZE - v->rbuflen,flags);
  if(r<=0) return r;

  // having just read, present new data to PP
  v->rbuflen += r;
  show_buffer(SYS_BUF_REFILLED,v);
  present_pp(v,s);
  //assert(v->tosend);
  return buf_ret(v,s,buf,len);
}

START_WRAP(ssize_t, recv,(int s, void *buf, size_t len, int flags))
{
  if(bb_disable_all) return real_recv(s, buf, len, flags);
  if (s < 0  ||  s >= MAX_FDS)
    return real_recv(s, buf, len, flags);

  ssize_t r;
  vfd_t *v = &vfd_tab[s];

  LOGBINiii(SYS_RECV_BEGIN,0,s, len, flags);
  if(v->pp_dispatcher) {
    r = virtual_recv(v,s,buf,len,flags);
    assert(v->rbufofs <= v->rbuflen);
  } else {
    r = real_recv(s,buf,len,flags);
    clear_readable_cache(s);
  }
  LOGBINiii(SYS_RECV,r,s, len, flags);

  return r;
  END_WRAP;
}

// START_WRAP(ssize_t, recvmsg,(int s, struct msghdr *msg, int flags))
// {
//   ssize_t r = real_recvmsg(s, msg, flags);
// 
//   if (s < 0  ||  s >= MAX_FDS  ||  !FD_ISSET(s, &tracked))
//     return r;
// 
//   if (pthread_mutex_lock_stub(&vsel_data.lock) != 0)
//     LOGBIN(SYS_FATAL_LOCK,-1,"recvmsg");
//   else {
//     FD_CLR(s, &vsel_data.read);
//     pthread_mutex_unlock_stub(&vsel_data.lock);
//   }
// 
//   LOGBINiii(SYS_,0,"%d=recvmsg(%d,%d)", r, s, flags);
//   return r;
//   END_WRAP;
// }

// ##########################################################
// ### writing                                            ###
// ##########################################################

START_WRAP(ssize_t, write,(int s, const void *buf, size_t len))
{
  if(bb_disable_all) return real_write(s, buf, len);
  if (s < 0  ||  s >= MAX_FDS)
    return real_write(s, buf, len);

  LOGBINii(SYS_WRITE_BEGIN,0,s, len);
  ssize_t r = real_write(s, buf, len);

  vwrite_req(&vfd_tab[s],s,buf,(int)r);

  if (pthread_mutex_lock_stub(&vsel_data.lock) != 0)
    LOGBINfatal("write");
  else {
    FD_CLR(s, &vsel_data.write);
    pthread_mutex_unlock_stub(&vsel_data.lock);
  }

  LOGBINii(SYS_WRITE,r,s, len);
  return r;
  END_WRAP;
}

START_WRAP(ssize_t, writev,(int s, const struct iovec *vec, int count))
{
  if(bb_disable_all) return real_writev(s, vec, count);
  if (s < 0  ||  s >= MAX_FDS) //  ||  !FD_ISSET(s, &tracked))
    return real_writev(s, vec, count);

  LOGBINii(SYS_WRITEV_BEGIN,0,s, count);
  int r = (int)real_writev(s, vec, count);

  if (r != -1) {
    vfd_t *v = &vfd_tab[s];
    if (r > 0  &&  v  &&  v->pp_dispatcher) {

      int v,todo = r;
      for(v = 0; v < count; v++) {
	int try = (vec[v].iov_len > todo ? todo : vec[v].iov_len);
	vwrite_req(&vfd_tab[s],s,vec[v].iov_base,try);
	todo -= try;
	if(todo == 0) break;
      }
    }
  }

  if (pthread_mutex_lock_stub(&vsel_data.lock) != 0)
    LOGBINfatal("writev");
  else {
    FD_CLR(s, &vsel_data.write);
    pthread_mutex_unlock_stub(&vsel_data.lock);
  }

  LOGBINii(SYS_WRITEV,r,s, count);
  return (ssize_t)r;
  END_WRAP;
}

START_WRAP(ssize_t, send,(int s, const void *buf, size_t len, int flags))
{
  if(bb_disable_all) return real_send(s, buf, len, flags);
  if (s < 0  ||  s >= MAX_FDS)
    return real_send(s, buf, len, flags);

  LOGBINiii(SYS_SEND_BEGIN,0,s, len, flags);
  ssize_t r = real_send(s, buf, len, flags);

  vwrite_req(&vfd_tab[s],s,buf,(int)r);

  if (pthread_mutex_lock_stub(&vsel_data.lock) != 0)
    LOGBINfatal("send");
  else {
    FD_CLR(s, &vsel_data.write);
    pthread_mutex_unlock_stub(&vsel_data.lock);
  }

  LOGBINiii(SYS_SEND,r,s, len, flags);
  return r;
  END_WRAP;
}

START_WRAP(ssize_t, sendto,(int  s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen))
{
  if(bb_disable_all) return real_sendto(s, buf, len, flags, to, tolen);
  if (s < 0  ||  s >= MAX_FDS)
    return real_sendto(s, buf, len, flags, to, tolen);

  LOGBINiii(SYS_SENDTO_BEGIN,0,s, len, flags);
  ssize_t r = real_sendto(s, buf, len, flags, to, tolen);

  vwrite_req(&vfd_tab[s],s,buf,(int)r);

  if (pthread_mutex_lock_stub(&vsel_data.lock) != 0)
    LOGBINfatal("sendto");
  else {
    FD_CLR(s, &vsel_data.write);
    pthread_mutex_unlock_stub(&vsel_data.lock);
  }

  LOGBINiii(SYS_SENDTO,r,s, len, flags);
  return r;
  END_WRAP;
}

START_WRAP(ssize_t, sendfile,(int out_fd, int in_fd, off_t *offset, size_t count))
{
  if(bb_disable_all) return real_sendfile(out_fd, in_fd, offset, count);
  if (out_fd < 0  ||  out_fd >= MAX_FDS)
    return real_sendfile(out_fd, in_fd, offset, count);

  LOGBINiii(SYS_SENDFILE_BEGIN,0,out_fd, in_fd, count);
  ssize_t r = real_sendfile(out_fd, in_fd, offset, count);

/*   if (r != -1) { */
/*     vfd_t *v = &vfd_tab[out_fd]; */
/*     if (v->pp_dispatcher) */
/*       v->pp_dispatcher->pp_write(v->pp_data, out_fd, NULL, r); */
/*   } */

  if (pthread_mutex_lock_stub(&vsel_data.lock) != 0)
    LOGBINfatal("sendfile");
  else {
    FD_CLR(out_fd, &vsel_data.write);
    pthread_mutex_unlock_stub(&vsel_data.lock);
  }

  LOGBINiii(SYS_SENDFILE,r,out_fd, in_fd, count);
  return r;
  END_WRAP;
}

START_WRAP(ssize_t, sendfile64,(int out_fd, int in_fd, off_t *offset, size_t count))
{
  if(bb_disable_all) return real_sendfile64(out_fd, in_fd, offset, count);
  if (out_fd < 0  ||  out_fd >= MAX_FDS)
    return real_sendfile64(out_fd, in_fd, offset, count);

  LOGBINiii(SYS_SENDFILE64_BEGIN,0,out_fd, in_fd, count);
  ssize_t r = real_sendfile64(out_fd, in_fd, offset, count);

/*   if (r != -1) { */
/*     vfd_t *v = &vfd_tab[out_fd]; */
/*     if (v->pp_dispatcher) */
/*       v->pp_dispatcher->pp_write(v->pp_data, out_fd, NULL, r); */
/*   } */

  if (pthread_mutex_lock_stub(&vsel_data.lock) != 0)
    LOGBINfatal("sendfi64");
  else {
    FD_CLR(out_fd, &vsel_data.write);
    pthread_mutex_unlock_stub(&vsel_data.lock);
  }

  LOGBINiii(SYS_SENDFILE64,r,out_fd, in_fd, count);
  return r;
  END_WRAP;
}

// START_WRAP(ssize_t, sendmsg,(int s, const struct msghdr *msg, int flags))
// {
//   int r = real_sendmsg(s, msg, flags);
// 
//   if (s < 0  ||  s >= MAX_FDS  ||  !FD_ISSET(s, &tracked))
//     return r;
// 
//   if (pthread_mutex_lock_stub(&vsel_data.lock) != 0)
//     LOGBINfatal("sendmsg");
//   else {
//     FD_CLR(s, &vsel_data.write);
//     pthread_mutex_unlock_stub(&vsel_data.lock);
//   }
// 
//   LOGBIN(SYS_,0,"%d=sendmsg(%d,%d)", r, s, flags);
//   return r;
//   END_WRAP;
// }

// ##########################################################
// select() and poll()

// We implement both wrappers as select-like calls.

int speak_select(int n, fd_set* readfds, fd_set* writefds,
		 fd_set* exceptfds, struct timeval* timeout, int *rfd) {

  int tofree[3]; tofree[0] = tofree[1] = tofree[2] = 0;
  int i, r = 0, q, fd = -1;
  char lbuf[2048], lrbuf[1024], lwbuf[1024], tmp[16];
  *rfd = -1; // safety

  if(!readfds) {
    readfds = malloc(sizeof(fd_set));
    FD_ZERO(readfds);
    tofree[0] = 1;
  }
  if(!writefds) {
    writefds = malloc(sizeof(fd_set));
    FD_ZERO(writefds);
    tofree[1] = 1;
  }
  if(!exceptfds) {
    exceptfds = malloc(sizeof(fd_set));
    FD_ZERO(exceptfds);
    tofree[2] = 1;
  }

  // Check if our cached fd_sets can satisfy this call
  int rwe = VFD_SEL_NONE;
  for(i=0;i<n;i++) {
    if(FD_ISSET(i,readfds) && FD_ISSET(i,&vsel_data.read))
      { fd = i; r = 1; rwe = VFD_SEL_READ; FD_CLR(i,&vsel_data.read); break; }
    if(FD_ISSET(i,writefds) && FD_ISSET(i,&vsel_data.write))
      { fd = i; r = 1; rwe = VFD_SEL_WRITE; FD_CLR(i,&vsel_data.write); break; }
    if(FD_ISSET(i,exceptfds) && FD_ISSET(i,&vsel_data.except))
      { fd = i; r = 1; rwe = VFD_SEL_EXCEPT; FD_CLR(i,&vsel_data.except); break; }
  }
  

  if(r) {
    if (pthread_mutex_lock_stub(&vsel_data.lock) != 0)
      { LOGBINfatal("select"); return -1; }
    // make sure none of the other requested slots are set
    FD_ZERO(readfds);
    FD_ZERO(writefds);
    FD_ZERO(exceptfds);
    // ... except for the one we found
    if(rwe == VFD_SEL_READ)
      FD_SET(fd,readfds);
    if(rwe == VFD_SEL_WRITE)
      FD_SET(fd,writefds);
    if(rwe == VFD_SEL_EXCEPT)
      FD_SET(fd,exceptfds);
    pthread_mutex_unlock_stub(&vsel_data.lock);
    goto sel_done;
  }

  // Our cache says none of the fds the user wants is set,
  // so we call real_select() to update our cache.
  r = real_select(n, readfds, writefds, exceptfds, timeout);

  if (pthread_mutex_lock_stub(&vsel_data.lock) != 0) {
    LOGBINfatal("select");
    return -1;
  }

  // this is bad because we will forget about things we're holding
  //   e.g. select(fd=4). select(fd=8). select(fd=4)
  //FD_ZERO(&vsel_data.read);
  //FD_ZERO(&vsel_data.write);
  //FD_ZERO(&vsel_data.except);

  // Update our cache of fd sets and send back the first
  // live one.
  if (r > 0) {

    if(r > 1)
      LOGBIN0(SYS_PP_ISO_POLL,r);

    // copy from (read|write)fds to vsel_data.(read|write)
    for(i=0;i<n;i++) {
      if(FD_ISSET(i,readfds))
	FD_SET(i,&vsel_data.read);
      if(FD_ISSET(i,writefds))
	FD_SET(i,&vsel_data.write);
      if(FD_ISSET(i,exceptfds))
	FD_SET(i,&vsel_data.except);
    }

    // find the first fd
    fd = -1;
    r = 1;
    // clear readfds, writefds, but set fd
    FD_ZERO(readfds);
    FD_ZERO(writefds);
    FD_ZERO(exceptfds);
    for(i=0;i<n;i++) {
      if(FD_ISSET(i,&vsel_data.read))
	{ fd = i; FD_SET(i,readfds); break; }
      if(FD_ISSET(i,&vsel_data.write))
	{ fd = i; FD_SET(i,writefds); break; }
      if(FD_ISSET(i,&vsel_data.except))
	{ fd = i; FD_SET(i,exceptfds); break; }
    }

  } else {
    fd = -1;
  }

  pthread_mutex_unlock_stub(&vsel_data.lock);

sel_done:
  if(tofree[0]) { free(readfds); readfds = NULL; }
  if(tofree[1]) { free(writefds); writefds = NULL; }
  if(tofree[2]) { free(exceptfds); exceptfds = NULL; }

  // maybe a connect event
  if(fd > -1 && vfd_tab[fd].in_prog)
    connect_event(fd,1);
  *rfd = fd;
  return r;
}

START_WRAP(int, select,(int n, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout))
{
  if(bb_disable_all) return real_select(n,readfds,writefds,exceptfds,timeout);
  int r, rfd;
  LOGBIN0(SYS_SELECT_BEGIN,0);
  r = speak_select(n,readfds,writefds,exceptfds,timeout,&rfd);
  LOGBINii(SYS_SELECT,r,n,rfd);
  return r;
  END_WRAP;
}

// START_WRAP(int,
//            pselect,(int n, fd_set* readfds, fd_set* writefds,
//                     fd_set* exceptfds, const struct timespec* timeout,
//                     const sigset_t* sigs))
// {
//   LOGBIN(SYS_,0,"pselect(%d)", n);
//   return real_pselect(n, readfds, writefds, exceptfds, timeout, sigs);
//   END_WRAP;
// }

// TODO: epoll_create, epoll_ctl, epoll_wait

START_WRAP(int, poll,(struct pollfd *ufds, nfds_t nfds, int timeout))
{
  if(bb_disable_all) return real_poll(ufds,nfds,timeout);
  int r, maxfd = 0, rfd = 0, i;

  LOGBINii(SYS_POLL_BEGIN,0,nfds, timeout);

  // convert timeout to tv
  struct timeval tvreal;
  struct timeval *tv;
  if(timeout >= 0) {
    tvreal.tv_sec = (time_t)(timeout / 1000);
    tvreal.tv_usec = (suseconds_t)(timeout - ((int)tvreal.tv_sec * 1000)) * 1000;
    tv = &tvreal;
    //fprintf(stderr,"POLL TO: %d --> %ld %ld\n",
    //timeout, (long)tv->tv_sec, (long)tv->tv_usec);
  }
  else
    //wait forever
    tv = NULL;

  // convert ufds into readfds,writefds
  fd_set readfds, writefds, exceptfds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  for (i = 0; i < nfds; i++) {
    int tfd = ufds[i].fd;
    //fprintf(stderr,"something about fd=%d : %X\n",tfd,ufds[i].events);
    ufds[i].revents = 0;
    if (ufds[i].events & POLLIN)
      FD_SET(tfd,&readfds);
    if (ufds[i].events & POLLOUT)
      FD_SET(tfd,&writefds);
    if (tfd > maxfd) maxfd = tfd;
  }

  r = speak_select(maxfd+1,&readfds,&writefds,&exceptfds,tv,&rfd);

  // set ufds.revents
  for(i=0;i<nfds;i++) {
    int tfd = ufds[i].fd;
    if (tfd != rfd) continue;
    if (FD_ISSET(tfd,&readfds)) {
      ufds[i].revents = POLLIN;
      break;
    }
    if (FD_ISSET(tfd,&writefds)) {
      ufds[i].revents = POLLOUT;
      break;
    }
  }

  LOGBINiii(SYS_POLL,r,nfds, timeout, rfd);
  return r;
  END_WRAP;
}

// ##########################################################
// fork/exec

START_WRAP(pid_t, fork,())
{
  if(debug_events && !bb_disable_all) LOGBIN0(SYS_FORK_BEGIN,0);
  if(!bb_disable_all) lb_flush();

  unsigned long long int forker_tid = get_tid();
  int forker_pid = get_pid();
  int r = real_fork();
  if (r != 0) {
    if(debug_events && !bb_disable_all) LOGBIN0(SYS_FORK,r);
  } else {
    fork_init();
    if(!bb_disable_all) LOGBINli(SYS_FORK,r,forker_tid,forker_pid);
  }

  return r;
  END_WRAP;
}

void setenv_carry(const char *path) {
  char *lib_path = real_getenv("LIBBTRACE_PATH");
  if (!lib_path) {
    lib_path = TRACE_LIB;
  }

  char tmp[32];
  sprintf(tmp,"%llu",get_tid());
  setenv("BB_EXEC_PTID", tmp, 1);
  setenv("BB_EXEC_PATH", path, 1);
  setenv("TRACE_FN", basename(path), 1);
  setenv("LD_PRELOAD", lib_path, 1);
  if(bb_disable_pp)
    setenv("BB_DISABLE_PP","yes",1);
  if(bb_disable_log)
    setenv("BB_DISABLE_LOG","yes",1);
  if(bb_disable_all)
    setenv("BB_DISABLE_ALL","yes",1);
}

START_WRAP(int, execve,(const char *filename, char *const argv[], char *const envp[]))
{
  char *lib_path = real_getenv("LIBBTRACE_PATH");
  if (!lib_path) {
    lib_path = TRACE_LIB;
  }

  // add LD_PRELOAD to the user-supplied envp
  //   so that the exec()'d process is traced
  // add BB_EXEC_PTID to the user-supplied envp
  //   so that we know which thread called exec()  
  int i,j;
  char ldp[1024], ept[1024], pa[1024], pn[1024];
  // malloc space for more
  for (i = 0; envp[i] != NULL; i++);
  char **mod_envp = (char **)calloc(sizeof(char *), i + 10);
  // put in my stuff first
  i = 0;
  sprintf(ldp, "LD_PRELOAD=%s", lib_path);
  mod_envp[i++] = ldp;
  sprintf(pn, "TRACE_FN=%s", basename(filename));
  mod_envp[i++] = ldp;
  sprintf(ept, "BB_EXEC_PTID=%llu", get_tid());
  mod_envp[i++] = ept;
  sprintf(pa, "BB_EXEC_PATH=%s", filename);
  mod_envp[i++] = pa;
  if(bb_disable_pp)
    mod_envp[i++] = "BB_DISABLE_PP=yes";
  if(bb_disable_all)
    mod_envp[i++] = "BB_DISABLE_ALL=yes";
  // copy envp
  for(j=0;envp[j]!=NULL;)
    mod_envp[i++] = envp[j++];
  mod_envp[i] = NULL;

  if(1/*debug_events*/) LOGBINis(SYS_EXEC,0,i,filename);
  if(!bb_disable_all) lb_flush();

  return real_execve(filename, argv, mod_envp);
  END_WRAP;
}

START_WRAP(int, execl,(const char *path, const char *arg, ...))
{
  ARG2ARGV(arg);

  setenv_carry(path);
  if(debug_events) LOGBINs(SYS_EXECL,0,path);
  if(!bb_disable_all) lb_flush();
  return real_execv(path, (char *const *) argv);

  END_WRAP;
}

START_WRAP(int, execlp,(const char *file, const char *arg, ...))
{
  ARG2ARGV(arg);

  setenv_carry(file);
  if(debug_events) LOGBINs(SYS_EXECLP,0,file);
  if(!bb_disable_all) lb_flush();
  return real_execvp(file, (char *const *) argv);
  END_WRAP;
}

START_WRAP(int, execle,(const char *path, const char *arg, ...))
     //char * const envp[]))
{
  ARG2ARGV(arg);

  setenv_carry(path);
  if(debug_events) LOGBINs(SYS_EXECLE,0,path);
  if(!bb_disable_all) lb_flush();
  return real_execv(path, (char *const *) argv);
  END_WRAP;
}

START_WRAP(int, execv,(const char *path, char * const argv[]))
{
  setenv_carry(path);
  if(debug_events) LOGBINs(SYS_EXECV,0,path);
  if(!bb_disable_all) lb_flush();
  return real_execv(path,(char *const *) argv);
  END_WRAP;
}

START_WRAP(int, execvp,(const char *file, char * const argv[]))
{
  setenv_carry(file);
  if(debug_events) LOGBINs(SYS_EXECVP,0,file);
  if(!bb_disable_all) lb_flush();
  return real_execvp(file,(char *const *) argv);
  END_WRAP;
}


// ##########################################################
// others

// START_WRAP(int, gettimeofday,(struct timeval* tv, struct timezone* tz))
// {
//   LOGBIN(SYS_,0,"gettimeofday()");
//   return real_gettimeofday(tv, tz);
//   END_WRAP;
// }

START_WRAP(char *, getenv,(const char *name))
{
  if(bb_disable_all) return real_getenv(name);
  char tmp[1024];
  //if (strncmp(name, "LD_PRELOAD", 10) == 0) return NULL;
  char * r = real_getenv(name);
  if(!name) return r;
  if(r)
    snprintf(tmp,1024,"%s=%s",name,r);
  else
    snprintf(tmp,1024,"%s=(undef)",name);

  if(debug_events) LOGBINs(SYS_GETENV,0,tmp);

  return r;
  END_WRAP;
}

START_WRAP(int, clearenv,())
{
  char *lib_path = real_getenv("LIBBTRACE_PATH");
  if (!lib_path) {
    lib_path = TRACE_LIB;
  }

  //if(bb_disable_all) return real_clearenv();
  int r = real_clearenv();
  setenv("LD_PRELOAD", lib_path, 1);
  if(bb_disable_pp)
    setenv("BB_DISABLE_PP","yes",1);
  if(bb_disable_log)
    setenv("BB_DISABLE_LOG","yes",1);
  if(bb_disable_all)
    setenv("BB_DISABLE_ALL","yes",1);
  if(debug_events) LOGBIN0(SYS_CLEARENV,r);
  return r;
  END_WRAP;
}

START_WRAP(int, unsetenv,(const char *name))
{
  char *lib_path = real_getenv("LIBBTRACE_PATH");
  if (!lib_path) {
    lib_path = TRACE_LIB;
  }

  //if(bb_disable_all) return real_unsetenv(name);
  int r = real_unsetenv(name);
  setenv("LD_PRELOAD", lib_path, 1);
  setenv("TRACE_FN", prog_name, 1);
  if(bb_disable_pp)
    setenv("BB_DISABLE_PP","yes",1);
  if(bb_disable_log)
    setenv("BB_DISABLE_LOG","yes",1);
  if(bb_disable_all)
    setenv("BB_DISABLE_ALL","yes",1);
  if(debug_events) LOGBINs(SYS_UNSETENV,0,name);
  return r;
  END_WRAP;
}

// ##########################################################

// pthread_create: in this wrapper, we need to call
//    alloc_ctx() to set up the thread-specific context
//    used for logging (see logbin.h).
// Since newly spawned threads execute 'start_routine',
//    we have to create a start_routine_wrapper, which
//    first calls alloc_ctx(), and then invokes the user-
//    supplied start_routine, with the user-supplied arg

typedef struct {
  void *(*fn)(void *);
  void *arg;
} sr_wrap_bag_t;

void * start_routine_wrapper(void *bag) {
  sr_wrap_bag_t* u = (sr_wrap_bag_t*)bag;
  void *res;
  logd_init(); // this calls alloc_ctx(mypid,logfd);
  if(!bb_disable_all) LOGBIN0(SYS_PTH_CREATE_CHILD,0);
  res = (u->fn)(u->arg);
  free(bag);
  free_ctx();
  if(!bb_disable_all) lb_flush();
  return res;
}

START_WRAP(int, pthread_create, (pthread_t * thread, const pthread_attr_t * attr, void * (*start_routine)(void *), void * arg))
{
  //if(bb_disable_all) return real_pthread_create(thread,attr,start_routine,arg);
  sr_wrap_bag_t *bag = malloc(sizeof(sr_wrap_bag_t));
  if(DEBUG) fprintf(stderr,"thread_create ...\n");
  if(!bb_disable_all) LOGBIN0(SYS_PTH_CREATE_BEGIN,0);
  bag->fn = start_routine;
  bag->arg = arg;
  int r = real_pthread_create(thread,attr,&start_routine_wrapper,(void *)bag);
  if(DEBUG) fprintf(stderr,"thread_create [%ld]\n",*thread);
  // TODO : Add an event here
  if(!bb_disable_all) LOGBINli(SYS_PTH_CREATE_PARENT,0,*thread,0);
  return r;
  END_WRAP;
}

START_WRAP(int, pthread_atfork, (void (*prepare)(void), void (*parent)(void), void (*child)(void)))
{
  if(bb_disable_all) return pthread_atfork(prepare,parent,child);
  fprintf(stderr,"you have threads that call fork(). may god help you.\n");
  real_exit(1);
  END_WRAP;
}

// ##########################################################
// mmap/munmap

// START_WRAP(void *,
//            mmap,(void *start, size_t length, int prot, int flags,
//                  int fd, off_t offset))
// {
//   LOGBIN(SYS_,0,"mmap(%d,%d)", fd, (int)offset);
//   return real_mmap(start, length, prot, flags, fd, offset);
//   END_WRAP;
// }

// START_WRAP(void *,
//            mmap2,(void *start, size_t length, int prot, int flags,
//                   int fd, off_t pgoffset))
// {
//   LOGBIN(SYS_,0,"mmap2(%d,%d)", fd, (int)pgoffset);
//   return real_mmap2(start,length,prot,flags,fd,pgoffset);
//   END_WRAP;
// }

// START_WRAP(int, munmap,(void *start, size_t length))
// {
//   LOGBIN(SYS_,0,"munmap()");
//   return real_munmap(start, length);
//   END_WRAP;
// }

