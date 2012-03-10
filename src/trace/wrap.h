//
// $Id: wrap.h,v 1.56 2007-06-26 20:20:59 ejk Exp $
//
// interposition macros
//

// ##########################################################

#ifndef WRAP_H
#define WRAP_H

// ##########################################################

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include "cycles.h"
#include "../logd/binlog.h"

// ##########################################################

extern long cyA,cyB,cyC,cyD,cyE;

enum boolean { FALSE = 0, TRUE };

typedef struct list_head {
  struct list_head *next;
  struct list_head *prev;
} list_t;

/* Thread descriptor data structure.  */
struct pthread
{
  union
  {
    struct
    {
      int multiple_threads;
    } header;

    void *__padding[16];
  };

  list_t list;

  /* Thread ID - which is also a 'is this thread descriptor (and
     therefore stack) used' flag.  */
  pid_t tid;
  /* Process ID - thread group ID in kernel speak.  */
  pid_t pid;
};

// ##########################################################

//grep START_WRAP trace.c | perl -n -e 'print if s/^START_WRAP\(([^,]+), ([^,]+),(\([^\)]*\))\) {/$1 (* real_$2) $3;/;'

void (* real_exit) (int status) __attribute__ ((noreturn));
void (* real_abort) () __attribute__ ((noreturn));

//int (* real_fstat) (int filedes, struct stat *buf);
int (* real_pipe) (int filedes[2]);
int (* real_dup) (int oldfd);
int (* real_dup2) (int oldfd, int newfd);
int (* real_fcntl) (int fd, int cmd, ...);
int (* real_socket) (int domain, int type, int protocol);
int (* real_bind) (int s, const struct sockaddr *my_addr, socklen_t addrlen);
int (* real_listen) (int s, int backlog);
int (* real_accept) (int s, struct sockaddr *addr, socklen_t *addrlen);
int (* real_connect) (int s, const struct sockaddr* serv_addr,
                      socklen_t addrlen);
int (* real_close) (int s);

// int (* real_getsockname) (int s, struct sockaddr *name, socklen_t *namelen);
// int (* real_getpeername) (int s, struct sockaddr *name, socklen_t *namelen);

int (* real_open) (const char *pathname, int flags);
int (* real_creat) (const char *pathname, mode_t mode);
ssize_t (* real_read) (int s, void *buf, size_t len);
ssize_t (* real_readv) (int fd, const struct iovec *vector, int count);
ssize_t (* real_recv) (int s, void *buf, size_t len, int flags);
ssize_t (* real_recvfrom) (int s, void *buf, size_t len, int flags,
                           struct sockaddr *from, socklen_t *fromlen);
// ssize_t (* real_recvmsg) (int s, struct msghdr *msg, int flags);

ssize_t (* real_write) (int s, const void *buf, size_t len);
ssize_t (* real_writev) (int s, const struct iovec *vector, int count);
ssize_t (* real_send) (int s, const void *buf, size_t len, int flags);
ssize_t (* real_sendto) (int s, const void *buf, size_t len, int flags,
                         const struct sockaddr*to, socklen_t tolen);
ssize_t (* real_sendfile) (int out_fd, int in_fd, off_t *offset,
                           size_t count);
ssize_t (* real_sendfile64) (int out_fd, int in_fd, off_t *offset,
                             size_t count);
// ssize_t (* real_sendmsg) (int s, const struct msghdr *msg, int flags);

int (* real_select) (int  n, fd_set* readfds, fd_set* writefds,
                     fd_set* exceptfds, struct timeval* timeout);
// int (* real_pselect) (int n, fd_set* readfds, fd_set* writefds,
//                      fd_set* exceptfds, const struct timespec* timeout,
//                      const sigset_t* sigs);
int (* real_poll) (struct pollfd* ufds, nfds_t nfds, int timeout);

pid_t (* real_fork) ();
int (* real_execve) (const char *filename, char *const argv [],
                     char *const envp[]);
int (* real_execl) (const char *path, const char *arg, ...);

int (* real_execlp)(const char *file, const char *arg, ...);
int (* real_execle)(const char *path, const char *arg, ...);
int (* real_execv)(const char *path, char *const argv[]);
int (* real_execvp)(const char *file, char *const argv[]);


// int (* real_gettimeofday) (struct timeval* tv, struct timezone* tz);
char *(* real_getenv) (const char *name);
int (* real_clearenv) ();
int (* real_unsetenv) (const char *name);

// void * (* real_mmap) (void *start, size_t length, int prot, int flags,
//                       int fd, off_t offset);
// void * (* real_mmap2)(void *start, size_t length, int prot, int flags,
//                       int fd, off_t pgoffset);
// int (* real_munmap)(void *start, size_t length);

int (* real_pthread_create) (pthread_t * thread, const pthread_attr_t * attr,
			     void * (*start_routine)(void *), void * arg);
int (* real_pthread_atfork) (void (*prepare)(void), void (*parent)(void),
			     void (*child)(void));

// #################################################################
//  fprintf(stderr,"hi " #f); fflush(stderr);

#define START_WRAP(r,f,args)			\
r f args {					\
  if (!first_init)                              \
    vfd_init( #f );
// LOGBINs(197,0, #f);

#define INIT_REAL(f)			        \
  if (! real_##f) {				\
    dlerror();                                  \
    real_##f = dlsym(((void *) -1l), #f);	\
    if (! real_##f) {				\
      char* err = dlerror();                    \
      fprintf(stderr, "dlsym: (" #f ") %s\n", err);\
      exit(1);	 				\
    }						\
  }

#define END_WRAP } 
//LOGBIN0(198,0); lb_flush(); }

// #################################################################

// http://www.google.com/codesearch?hl=en&q=+execl+show:EBbqZc883-c:yLMn06NsZP4:1TF0IiDcW9g&sa=N&cd=5&ct=rc&cs_p=http://gentoo.osuosl.org/distfiles/glibc-2.3.5.tar.bz2&cs_f=glibc-2.3.5/posix/execl.c#a0

#define INITIAL_ARGV_MAX 1024

#define ARG2ARGV(last_real_arg) \
  va_list args; \
  size_t argv_max = INITIAL_ARGV_MAX; \
  const char *initial_argv[INITIAL_ARGV_MAX]; \
  const char **argv = initial_argv; \
  argv[0] = last_real_arg; \
  va_start (args, last_real_arg); \
  unsigned int i = 0; \
  while (argv[i++] != NULL) { \
    if (i == argv_max) { \
      argv_max *= 2; \
      const char **nptr = realloc (argv == initial_argv ? NULL : argv, \
				   argv_max * sizeof (const char *)); \
      if (nptr == NULL) { \
	if (argv != initial_argv) free (argv); \
	return -1; \
      } \
      if (argv == initial_argv) \
	memcpy (nptr, argv, i * sizeof (const char *)); \
      argv = nptr; \
    } \
    argv[i] = va_arg (args, const char *); \
  } \
  va_end (args);

// #################################################################

#endif
