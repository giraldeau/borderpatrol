
#include "binlog.h"
#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <linux/limits.h>

// ##########################################################

#define BOK  1
#define BEOF 666

// keep a buffer
char lb_buf[PIPE_BUF+1];
int lb_cur = 0, lb_last = -1; // <-- grr

int bytes_seen = 0;

short refill(int fd, int persist) {
  int r;
  if(BUF_DEBUG) fprintf(stderr,"refill: last byte: %c\n",lb_buf[lb_last]);
  lb_cur = 0; lb_last = -1;
  do {
    r = read(fd,lb_buf,PIPE_BUF);
    if(r == -1) {
      perror("read_error"); exit(1);
    } else if(r == 0) {
      if(BUF_DEBUG) fprintf(stderr,"called for a refill, but got nothing :-/\n");
      if (persist)
        usleep(10*100);
      else
        return BEOF;
    } else {
      // fprintf(stderr,"refill: read %d bytes\n",r);
      lb_last = (r - 1);
    }
  } while (r <= 0);
  if(BUF_DEBUG) fprintf(stderr,"refill: first byte: %c\n",lb_buf[0]);
  return BOK;
}

short fill(int fd, int persist, char *thing,int num) {

  if(BUF_DEBUG && num != 16) fprintf(stderr,"fill(%d)\n",num);
  while(num > 0) {
    // use existing
    if(lb_last >= lb_cur) {
      int avail = lb_last - lb_cur + 1;
      if(BUF_DEBUG) fprintf(stderr,"avail(%d)\n",avail);
      int tocopy = (num < avail ? num : avail);
      memcpy(thing,&lb_buf[lb_cur],tocopy);
      num     = num - tocopy;          // need less now
      thing   = &thing[tocopy]; //   slide pointer
      lb_cur += tocopy;          // advance 'lb_cur'
      bytes_seen += tocopy;
    }

    if(num > 0) {
      if(BUF_DEBUG) fprintf(stderr,"refill for %d bytes!\n", num);
      if(refill(fd,persist) == BEOF)
        return BEOF;
    }
  }
  return BOK;
}

// ##########################################################

// returns NULL on eof

logentry_thaw_t *get_le(int fd,int persist) {
  char *buf;
  short r;

  // part 1
  int sz = sizeof(logentry_frozen_t);
  buf = malloc(sz);
  r = fill(fd,persist,buf,sz);
  if(r == BEOF) return NULL;
  logentry_frozen_t *fz = thaw1(buf);
  free(buf);

  // part 2
  sz = frozen_size(fz) - sizeof(logentry_frozen_t);
  buf = malloc(sz);
  r = fill(fd,persist,buf,sz);
  assert(r != BEOF);
  logentry_thaw_t *th = thaw2(buf,fz,sz);
  free(buf);

  return th;
}

void free_le(logentry_thaw_t *le) {
  if(le->fz.str_len > 0 && le->str)
    free(le->str);
  free(le);
}


// ##########################################################

// void thaw(char *buf, 
