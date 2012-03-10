#include "fmt.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "sysrender.h"

// ##########################################################

int predict_size(int arg_len, int str_len) {
  return sizeof(logentry_frozen_t)
    + ((arg_len)*sizeof(int))
    + str_len;
}
  

int frozen_size(logentry_frozen_t *fz) {
  return predict_size(fz->arg_len,fz->str_len);
}

void freeze(char *buf, logentry_thaw_t *le, int size) {
  int i;
  char *cur = buf;
  memcpy(cur,(char*)&(le->fz),sizeof(logentry_frozen_t));
  cur += sizeof(logentry_frozen_t);

  for(i = 0;i<le->fz.arg_len;i++) {
    memcpy(cur,(char*)&le->arg[i],sizeof(int));
    cur += sizeof(int);
  }

  if(le->fz.str_len) {
    memcpy(cur,le->str,le->fz.str_len);
    cur += le->fz.str_len;
  }

  assert(size == cur - buf);
}

// ##########################################################

logentry_frozen_t * thaw1(const char *buf) {
  logentry_frozen_t *out = malloc(sizeof(logentry_frozen_t));
  memcpy((char *)out, buf, sizeof(logentry_frozen_t));
  return out;
}

logentry_thaw_t* thaw2(char *buf, logentry_frozen_t *le, int rest_sz) {
  int i;
  char *cur = buf;
  int slen = le->str_len;
  logentry_thaw_t *out = malloc(sizeof(logentry_thaw_t));
  out->fz = *le;

  for(i = 0;i<out->fz.arg_len;i++) {
    memcpy((char*)&out->arg[i],cur,sizeof(int));
    cur += sizeof(int);
  }
  for(; i<3;i++) {
    out->arg[i] = 0;
  }

  if(slen) {
    out->str = malloc(slen+1);
    memcpy(out->str,cur,slen);
    out->str[slen] = '\0';
    cur += slen;
  }

  //fprintf(stderr,"%d,%d,%d,%d\n",rest_sz,cur,buf,cur - buf);
  assert(rest_sz == cur - buf);
  
  return out;
}

// ##########################################################

void output_le(FILE *out, logentry_thaw_t *l) {
  fprintf(out,"%llu|%d|%u,%u|%s|%d|%c|arg(%d,%d,%d),str(%d:%s)\n",
	  l->fz.tid,
	  l->fz.pid,
	  (uint32_t)l->fz.cycle_high,
	  (uint32_t)l->fz.cycle_low,
	  syscall2string(l->fz.sc),
	  l->fz.rv,
	  l->fz.nm,
	  l->arg[0],
	  l->arg[1],
	  l->arg[2],
	  l->fz.str_len,
	  (l->fz.str_len > 0 ? l->str : "")
	 );
}

void debug_le(logentry_thaw_t *l) {
  output_le(stderr,l);
}

void debug_ls_fz(logentry_frozen_t *l) {
  if (!l) {
    return;
  }

  logentry_thaw_t th;

  memcpy(&(th.fz), l, sizeof(logentry_frozen_t));
  th.str = NULL;
  th.arg[0] = 0;
  th.arg[1] = 0;
  th.arg[2] = 0;

  debug_le(&th);
}

void check_le(logentry_thaw_t *le) {
  if (le->fz.pid < 200 || le->fz.pid > 99999) {
    fprintf(stderr,"ERROR: check_le: string pid [%d]\n",le->fz.pid); exit(1);
  } else if (le->fz.tid < 200){
    fprintf(stderr,"ERROR: check_le: string tid [%llu]\n",(long long unsigned int)le->fz.tid); exit(1);
  } else if (le->fz.sc < 0 || le->fz.sc > 200) {
    fprintf(stderr,"ERROR: check_le: string sc [%d]\n",le->fz.sc); exit(1);
  }
}

// ##########################################################
