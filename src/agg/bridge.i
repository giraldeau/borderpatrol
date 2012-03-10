%module bridge
%{
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../logd/binlog.h"
#include "../logd/common.h"
#include "../logd/fmt.h"
#include "../trace/getservice.h"

int gl = 0;

logentry_thaw_t* next = NULL;

extern char bb_disable_log;

#define DEBUG_BRIDGE 0

typedef enum { isEof = 0, isNotEof = 1 } eof_t;

int binlog_open(char *fn) {
  int r;
  r = open(fn,O_RDONLY);
  if(DEBUG_BRIDGE) printf("open stream [%s] : %d\n",fn,r);
  return r;
}

eof_t binlog_eof(int fd) {
  if(next != NULL) {
    if(DEBUG_BRIDGE) output_le(stderr,next);
    free_le(next);
  }
  next = get_le(fd,0);
  if (next == NULL)
	  return isEof;
  else
	  return isNotEof;
}

logentry_thaw_t* binlog_next(int fd) {
  return next;
}

void binlog_close(int fd) {
  close(fd);
  if(DEBUG_BRIDGE) printf("closed %d\n",fd);
}

int binlog_mkpid(logentry_thaw_t* le) {
  return (int)le->fz.pid;
}
unsigned long long binlog_mktid(logentry_thaw_t* le) {
  return (unsigned long long)le->fz.tid;
}

unsigned long binlog_mkts(logentry_thaw_t* le) {
  // borealis says:
  //return (le->fz.cycle_high << 21) + (le->fz.cycle_low >> 11);
  return le->fz.cycle_high;

  // ejk:
  //unsigned long out = (unsigned long long)le->core.cycle_high;
  //out = out << 16; out += ((unsigned long long)le->core.cycle_low >> 16);
  //printf("%15lu h %15lu l %15lu r\n",
  //le->core.cycle_high, le->core.cycle_low, (unsigned long)out);
  //return (unsigned long)out;
}

int binlog_arg1(logentry_thaw_t* le) { return le->arg[0]; }
int binlog_arg2(logentry_thaw_t* le) { return le->arg[1]; }
int binlog_arg3(logentry_thaw_t* le) { return le->arg[2]; }

//char *binlog_mk_sockaddr(logentry_t* le) {
//char *res = parse_sockaddr(le->sas[mk_sockaddr_which++]);
// return res;
//}

// =====================================================================

%}

%include "../logd/fmt.h"

typedef enum { isEof = 0, isNotEof = 1 } eof_t;

int getportbyname(const char *name);
int binlog_open(char *fn);
eof_t binlog_eof(int fd);
logentry_thaw_t* binlog_next(int fd);
void binlog_close(int fd);

int binlog_mkpid(logentry_thaw_t* le);
unsigned long long binlog_mktid(logentry_thaw_t* le);
unsigned long binlog_mkts(logentry_thaw_t* le);
int binlog_arg1(logentry_thaw_t* le);
int binlog_arg2(logentry_thaw_t* le);
int binlog_arg3(logentry_thaw_t* le);
//char *binlog_mk_sockaddr(logentry_t* le);
