%module bridge
%{
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "/research/instance/active/src/logd/binlog.h"
#include "/research/instance/active/src/logd/common.h"
#include "/research/instance/active/src/logd/fmt.h"

int gl = 0;

int open_stream(char *fn) {
  int r;
  r = open(fn,O_RDONLY);
  printf("open stream [%s] : %d\n",fn,r);
  return r;
}

logentry_t* get_logentry(int fd) {
  logentry_t *le = get_le(fd,0);
  if(le != NULL) {
    printf("get_le(): ");
    output_le(stdout,le);
    return (void *) le;
  } else {
    printf("get_le(): NULL\n");
    return NULL;
  }
}

void free_logentry(logentry_t *le) {
  free_le((logentry_t *)le);
  printf("free_logentry()\n");
}

void close_stream(int fd) {
  close(fd);
  printf("closed %d\n",fd);
}

int expand(int arg) {
  printf("expand(%d) called in C. gl=%d\n",arg,gl++);
  return arg+1;
}

typedef struct {
  int yna;
} ejk;

%}

%include "/research/instance/active/src/logd/fmt.h"

int open_stream(char *fn);
void* get_logentry(int fd);
void free_logentry(void *le);
void close_stream(int fd);
int expand(int arg);

typedef struct {
  int yna;
} ejk;
