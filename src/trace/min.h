//
// $Id: min.h,v 1.3 2006-09-27 22:56:30 ejk Exp $
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
#include <linux/unistd.h>

extern char *prog_name;
extern int pipefd;

// ##########################################################

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

void (* real_exit) (int status) __attribute__ ((noreturn));
void (* real_abort) () __attribute__ ((noreturn));


pid_t (* real_fork) ();
int (* real_execve) (const char *filename, char *const argv [],
                     char *const envp[]);
int (* real_execl) (const char *path, const char *arg, ...);


// #################################################################

#define LOG(str, rest...) \
  do { \
    char outs[MAX_LOG_LINE+1]; \
    struct timeval tv; \
    gettimeofday(&tv,NULL); \
    snprintf(outs,MAX_LOG_LINE,"%lu|%d|%010li.%06li|%s|" str "\n", \
             1,getpid(),tv.tv_sec,tv.tv_usec,"prog_name", ## rest); \
    if(strnlen(outs,MAX_LOG_LINE) > PIPE_BUF) fprintf(stderr,"big!!!!!!!\n");\
    write(pipefd,outs,strnlen(outs,MAX_LOG_LINE)); \
  } while(0)

//    struct pthread *ptr = (struct pthread *)pthread_self(); \
//    if(!ptr->tid) perror("pthread"); \

// #################################################################

//printf("START_WRAP: " #f "\n");

#define START_WRAP(r,f,args)			\
r f args {                                      \
  if (!first_init)                              \
    init();

#define INIT_REAL(f)			        \
  if (! real_##f) {				\
    dlerror();                                  \
    real_##f = dlsym(RTLD_NEXT, #f);		\
    if (! real_##f) {				\
      char* err = dlerror();                    \
      fprintf(stderr, "dlsym: (" #f ") %s\n", err);\
      exit(1);					\
    }						\
  }

#define END_WRAP }

// #################################################################

#endif
