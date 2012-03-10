//
// $Id: min.c,v 1.4 2009-02-16 03:41:37 ning Exp $
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
#include <string.h>

#include "min.h"
#include "config.h"

typedef enum {FALSE = 0, TRUE = 1} boolean;

int first_init = 0;
int seq_no = 0;
char *prog_name;
char bin_name[1024];
int pipefd = -1;

struct sockaddr_in logd_addr;

void init();
void reinit();

// #################################################################

// FORK
START_WRAP(pid_t, fork,()) {
  int res;
  long ts;
  LOG("fork_begin()", res);
  res = real_fork();
  if(res == 0) {
    //reinit();
    //pp_init_log();
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

  int res = real_execve(filename,argv,hacked_envp);
  LOG("execve(\"%s\")",filename);
  return res;
  END_WRAP;
}

START_WRAP(int, execl,(const char *path, const char *arg, ...)) {

  LOG("execl(\"%s\")", path );
  return real_execl(path,arg);
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

void init() {
  int pf;
  char buf[24];
  struct timeval tv;
  int pid = getpid();
  if(!first_init) {

    //grep START_WRAP trace.c | perl -n -e 'chomp; print if s/^START_WRAP\(([^,]+), ([^,]+),(\([^\)]*\))\) {/INIT_REAL($2); /;'
    INIT_REAL(fork);
    INIT_REAL(execve);
    INIT_REAL(execl);

    INIT_REAL(exit);
    INIT_REAL(abort); 

    first_init = 1;
    seq_no = 0;

    if( (pipefd=open(LOGD_PIPE, O_WRONLY)) < 0 ) {
      fprintf(stderr,"couldn't connect to named pipe: %s\n", LOGD_PIPE);
      fflush(stderr); exit;
    }
    //prog_name[0] = '\0';
    //prog_name = getenv("TRACE_FN");
    LOG("start()");
  }
}

void reinit() {
  first_init = 0;
  if (pipefd > -1) close(pipefd);
  init();
}
