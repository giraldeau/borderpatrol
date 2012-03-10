#ifndef FMT_H
#define FMT_H

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>

typedef struct {
  uint64_t tid;
  uint32_t pid;
  uint32_t cycle_high;
  uint32_t cycle_low;
  short sc;                   /* System call identifier */
  short rv;                   /* Integer return value */
  char  nm;                     /* Program name */
  char  arg_len;                /* Number of arguments */
  short str_len;              /* Length of the additional string in
                                 * logentry_thaw_t */
  char _padding[4];             /* Padding to make the struct aligned, not
                                 * supposed to be used. */
} logentry_frozen_t;

typedef struct {
  logentry_frozen_t fz;
  char *str;                    /* Additional explanatory string */
  // arguments
  int arg[3];                   /* Arguments passed */
} logentry_thaw_t;

int predict_size(int arg_len, int str_len);
int frozen_size(logentry_frozen_t *fz);
void freeze(char *buf, logentry_thaw_t *le, int size);
logentry_frozen_t* thaw1(const char *buf);
logentry_thaw_t* thaw2(char *buf, logentry_frozen_t *le, int size);

void output_le(FILE *out, logentry_thaw_t *l);
void debug_le(logentry_thaw_t *l);
void check_le(logentry_thaw_t *le);

#endif
