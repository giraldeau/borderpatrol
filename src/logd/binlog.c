/*
 * File : binlog.c
 * Desc : Binary logging
 * Auth : Eric Koskinen
 * Date : Thu Nov  9 23:54:56 2006
 * RCS  : $Id: binlog.c,v 1.32 2009-04-03 00:32:58 ning Exp $
 */

#define _GNU_SOURCE

#include "binlog.h"
//#include "config.h"
#include "../trace/cycles.h"


#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>

#define DEBUG_BL 0

char *prog_name = "_";

// ##########################################################

int bb_disable_log = 0;
int bb_disable_log_getenv = 0;

/* Supposedly, if the environment variable BB_DISABLE_LOG is set, nothing will
 * be logged. */
int bb_disable_log_check() {
  return 0;
  if(bb_disable_log_getenv)
    return bb_disable_log;

  // this will loop forever
  if(getenv("BB_DISABLE_LOG")) 
    bb_disable_log = 1;

  bb_disable_log_getenv = 1;

  return bb_disable_log;  
}

// ##########################################################

#ifdef TRACK_PER_TID
void vfd_ctx_init(thread_ctx_t *ctx)
{
  FD_ZERO(&(ctx->tracked));
  FD_SET(0, &(ctx->tracked));
  FD_SET(1, &(ctx->tracked));
  FD_SET(2, &(ctx->tracked));

  int i;
  for (i = 0;  i < MAX_FDS;  i++) {
    if (ctx->vfd_tab[i].pp_dispatcher)
      ctx->vfd_tab[i].pp_dispatcher->pp_shutdown(&(ctx->vfd_tab[i].pp_data));
    assert(0);
    memset(&(ctx->vfd_tab[i]), 0, sizeof(vfd_t));
    // pthread_mutex_init(&vfd_tab[i].poll_events_lock, NULL);
  }
  
  vsel_t *vs = &(ctx->vsel_data);

  memset(vs, 0, sizeof(vsel_t));
  FD_ZERO(&(vs->read));
  FD_ZERO(&(vs->write));
  pthread_mutex_init(&(vs->lock), NULL);
}
#endif

// ##########################################################

static pthread_key_t ctx_key;
static pthread_once_t ctx_key_once = PTHREAD_ONCE_INIT;

static void ctx_destroy(void *buf) { free(buf); }
static void ctx_key_alloc() {
  pthread_key_create(&ctx_key, ctx_destroy);
}
void alloc_ctx(pid_t pid, int lfd) {
  thread_ctx_t *ctx;
  pthread_once(&ctx_key_once, ctx_key_alloc);
  ctx = malloc(sizeof(thread_ctx_t));
  ctx->lbptr = 0;
  ctx->lbcnt = 0;
  ctx->tid = pthread_self();
  ctx->pid = pid;
  ctx->logfd = lfd;
  ctx->fz_high = 0;
  ctx->fz_low = 0;

#ifdef TRACK_PER_TID
  vfd_ctx_init(ctx);
#endif
  
  pthread_setspecific(ctx_key, ctx);
}

thread_ctx_t *get_ctx() {
#ifdef DEBUG_GET_SPEC
  {
    thread_ctx_t *tc = (thread_ctx_t *)pthread_getspecific(ctx_key);
    FILE *tmp = fopen("/tmp/tids2","a");
    fprintf(tmp,"tid: %lu | self: %lu | pid: %d\n",tc->tid,pthread_self(),getpid());
    fclose(tmp);
    ctx_destroy(tc);
  }
#endif
  return (thread_ctx_t *)pthread_getspecific(ctx_key);
}

void free_ctx() {
  //free((thread_ctx_t *)pthread_getspecific(ctx_key));
}  

char *get_lb()    { return (get_ctx())->lb; }
int  *get_lbptr() { return &((get_ctx())->lbptr); }
int  *get_lbcnt() { return &((get_ctx())->lbcnt); }
unsigned long long int get_tid() { return (get_ctx())->tid; }
pid_t get_pid()   { return (pid_t)((get_ctx())->pid); }
int  *get_logfd() { return &((get_ctx())->logfd); }
unsigned long *get_fz_high() { return &((get_ctx())->fz_high); }
unsigned long *get_fz_low() { return &((get_ctx())->fz_low); }

#ifdef TRACK_PER_TID
fd_set *get_tracked()   { return &((get_ctx())->tracked); }
vsel_t *get_vsel_data() { return &((get_ctx())->vsel_data); }
vfd_t  *get_vfd_tab(int i) {
  if(i>MAX_FDS) {
    fprintf(stderr,"binlog: MAX_FD error\n"); fflush(stderr); exit(1);
  }
  return &((get_ctx())->vfd_tab[i]);
}
#endif

// ##########################################################

ssize_t (* r_write) (int s, const void *buf, size_t len);

void lb_flush() {
  PROF_START(lb_flush_a0);
  PROF_END(lb_flush_a0);
  if(bb_disable_log_check()) return;
  PROF_START(lb_flush_a);

  char   *lb = get_lb();     /* thread-local */
  int *lbptr = get_lbptr();  /* thread-local */
/*   int *lbcnt = get_lbcnt();  /\* thread-local *\/ */
  int *logfd = get_logfd();  /* thread-local */

  PROF_END(lb_flush_a);

  PROF_START(lb_flush_b);
  int start = 0;
  int amt = 0;
  if (! r_write ) {
    dlerror();
    r_write = dlsym(RTLD_NEXT, "write");
    if (! r_write ) {
      char* err = dlerror();
      fprintf(stderr, "binlog.c: dlsym: (write) %s\n", err);
      fflush(stderr); exit(1);
    }
  }

  while(start < *lbptr) {
    amt = (int)(&lb[*lbptr] - &lb[start]);
    int r = r_write(*logfd,&lb[start],amt);
    if (r == -1) {
      fprintf(stderr,"logbuf_flush ERROR! (fd=%d,tid=%llu)", *logfd, get_tid());
      perror("lb_flush");
      fflush(stderr);
      exit(1);
    }
    //if (DEBUG_BL) fprintf(stderr,"flush %d bytes\n", r);
    start += r;
  }

  //if (DEBUG_BL) fprintf(stderr,"total: %d records\n", *lbcnt);

  *lbptr = 0;
  PROF_END(lb_flush_b);
}

// ##########################################################

char *lb_alloc_bytes(int bytes) {
  PROF_START(lb_alloc_bytes);
  //fprintf(stderr,"alloc|%d\n",bytes);
  char   *lb = get_lb();    /* thread-local */
  int *lbptr = get_lbptr(); /* thread-local */

  if(bytes > PIPE_BUF) {
    fprintf(stderr,"alloc too big [%d]\n",bytes);
    fflush(stderr);
    exit(1);
  }
  if((*lbptr) + bytes > PIPE_BUF - 1) {
    lb_flush();
  }
  char *out = &lb[*lbptr];
  (*lbptr) += bytes;
  PROF_END(lb_alloc_bytes);
  return out;
}

logentry_thaw_t * lb_alloc_entry(int arg_len, int str_len) {
  return (logentry_thaw_t *)lb_alloc_bytes(predict_size(arg_len,str_len));
}

void SETARG(logentry_thaw_t *th,int arg, int val) {
  char *where = (char*)th + sizeof(logentry_frozen_t) + (arg-1)*sizeof(int);
  memcpy(where,(char *)&val,sizeof(int));
}

void SETSTR(logentry_thaw_t *th,const char *s,int slen) {
  char *where = (char*)th + sizeof(logentry_frozen_t) + th->fz.arg_len*sizeof(int);
  memcpy(where,s,slen);
  int i;
  for(i=0;i<slen;i++)
    if(where[i] == '\n') where[i] = '_';

}

// ##########################################################

void lb_freeze_current_time() {
  struct timeval t;
  unsigned long *h = get_fz_high();
  unsigned long *l = get_fz_low();

  gettimeofday(&t, NULL);
  *h = (unsigned long)t.tv_sec;
  *l = (unsigned long)t.tv_usec;
/*   *h = (unsigned long)cycle_high(); */
/*   *l = (unsigned long)cycle_low(); */
}

void lb_thaw_current_time() {
  unsigned long *h = get_fz_high();
  unsigned long *l = get_fz_low();
  *h = 0;
  *l = 0;
}

void lb_populate(logentry_frozen_t *fz, int sc, int rv) {
  PROF_START(lb_populate);
  unsigned long *h = get_fz_high();
  unsigned long *l = get_fz_low();
  struct timeval t;
  gettimeofday(&t, NULL);

  //#define FSTAT_ON_EACH_POPULATE 1
#ifdef FSTAT_ON_EACH_POPULATE
  {
    struct stat tmp;
    int *fd = get_logfd();
    if(fstat(*fd,&tmp) < 0) {
      fprintf(stderr,"lb_populate: %d no good. pid=%d tid=%llu sc=%d\n",
	      *fd, get_pid(), (unsigned long long int)get_tid(), sc);
      perror("lb_populate");
      exit(1);
    }
  }
#endif

  fz->nm         = prog_name[0];

  fz->cycle_high = (*h == 0 ? (unsigned long)t.tv_sec : *h);
  fz->cycle_low  = (*l == 0 ? (unsigned long)t.tv_usec  : *l);
  fz->pid        = get_pid();
  fz->tid        = get_tid();
  fz->sc         = sc;
  fz->rv         = rv;

  //th->arg[0]        = 0;
  //th->arg[1]        = 0;
  //th->arg[2]        = 0;
  //th->str           = NULL;
  
  fz->arg_len    = 0;
  fz->str_len    = 0;


  PROF_END(lb_populate);
  return;
}


// ##########################################################

// User Calls

void LOGBIN_gen(int sc, int rv, int _i1, int _i2, int _i3, int num, const char *s) {
  if(bb_disable_log_check()) return;
  int slen = (s == NULL ? 0 : strlen(s));
  logentry_thaw_t *th = lb_alloc_entry(num,slen);
  lb_populate(&(th->fz),sc,rv);
  th->fz.arg_len = num;
  th->fz.str_len = slen;
  SETSTR(th,s,slen);
  if(num >= 1) SETARG(th,1,_i1);
  if(num >= 2) SETARG(th,2,_i2);
  if(num >= 3) SETARG(th,3,_i3);
  //debug_le(th);
  //lb_flush();
}

void LOGBIN0(int sc, int rv) {
  if(bb_disable_log_check()) return;
  LOGBIN_gen(sc,rv,0,0,0,0,NULL);
}

void LOGBINli(int sc, int rv, pthread_t _ptid, int ppid) {
  if(bb_disable_log_check()) return;
  uint64_t temp_ptid = _ptid;
  char tmp[24];
  sprintf(tmp,"%llu", temp_ptid);
  LOGBIN_gen(sc,rv,ppid,0,0,1,tmp);
}

void LOGBINs(int sc, int rv, const char *s) {
  if(bb_disable_log_check()) return;
  LOGBIN_gen(sc,rv,0,0,0,0, s);
}

void LOGBINss(int sc, int rv, char *s1, char *s2) {
  int s1len = strlen(s1)+1;
  int s2len = strlen(s2)+1;
  int slen = s1len+s2len+4;
  char *s = malloc(slen);
  strncpy(s,s1,s1len+1);
  strcat(s,"=-=");
  strncat(s,s2,s2len+1);
  LOGBINs(sc,rv,s);
  free(s);
}

void LOGBINis(int sc, int rv, int _i1, const char *s) {
  LOGBIN_gen(sc,rv,_i1,0,0,1,s);
}


void LOGBINiis(int sc, int rv, int _i1, int _i2, const char *s) {
  LOGBIN_gen(sc,rv,_i1,_i2,0,2,s);
}

void LOGBINiiis(int sc, int rv, int _i1, int _i2, int _i3, const char *s) {
  LOGBIN_gen(sc,rv,_i1,_i2,_i3,3,s);
}

void LOGBINiii(int sc, int rv, int _i1, int _i2, int _i3) {
  LOGBIN_gen(sc,rv,_i1,_i2,_i3,3,NULL);
}

void LOGBINii(int sc, int rv, int i1, int i2) {
  LOGBIN_gen(sc,rv,i1,i2,0,2,NULL);
}

void LOGBINi(int sc, int rv, int i1) {
  LOGBIN_gen(sc,rv,i1,0,0,1,NULL);
}

void LOGBINfatal(const char *s) {
  LOGBINs(-1,-1,s);
}

/* void LOGBINia(int sc, int rv, int _i1, const struct sockaddr *sa1) { */
/*   if(bb_disable_log_check()) return; */
/*   char *blob = lb_alloc_bytes(sizeof(logentry_t) + sizeof(struct sockaddr)); */
/*   logentry_t *out = (logentry_t *)blob; */

/*   lb_populate(out,sc,rv); */
/*   out->i1 = _i1; */
/*   out->num_sa = 1; */
/*   memcpy(&blob[sizeof(logentry_t)], sa1, sizeof(struct sockaddr)); */
/*   out->sas[0] = (struct sockaddr *)&blob[sizeof(logentry_t)]; */
/*   if(DEBUG_BL) debug_le(out); */
/* } */

/* void LOGBINiaa(int sc, int rv, int _i1, */
/* 	       const struct sockaddr *sa1, const struct sockaddr *sa2) { */
/*   if(bb_disable_log_check()) return; */
/*   char *blob = lb_alloc_bytes(sizeof(logentry_t) + 2*sizeof(struct sockaddr)); */
/*   logentry_t *out = (logentry_t *)blob; */

/*   lb_populate(out,sc,rv); */
/*   out->i1 = _i1; */
/*   out->num_sa = 2; */
/*   memcpy(&blob[1*sizeof(logentry_t)], sa1, sizeof(struct sockaddr));   */
/*   memcpy(&blob[2*sizeof(logentry_t)], sa2, sizeof(struct sockaddr));   */
/*   out->sas[0] = (struct sockaddr *)&blob[1*sizeof(logentry_t)]; */
/*   out->sas[1] = (struct sockaddr *)&blob[2*sizeof(logentry_t)]; */
/*   if(DEBUG_BL) debug_le(out); */
/* } */

// ##########################################################
