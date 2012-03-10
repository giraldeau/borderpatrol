/*
 * File : binlog.h
 * Desc : Binary logging
 * Auth : Eric Koskinen
 * Date : Thu Nov  9 23:54:56 2006
 * RCS  : $Id: binlog.h,v 1.16 2009-04-03 00:32:58 ning Exp $
 */

// ##########################################################

#ifndef BINLOG_H
#define BINLOG_H

#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <linux/limits.h>
#include "../trace/config.h"
#include "../trace/protocol.h"
#include "../trace/vfd.h"
#include "fmt.h"

// ##########################################################

extern char *prog_name;
extern int mypid;

extern char lb[PIPE_BUF];
extern int  lbptr;
extern int  logfd;

// ##########################################################

typedef struct {
  pid_t pid;                    /* Process ID */
  int logfd;
  unsigned long long int tid;        /* Thread ID */
  int lbptr;
  char lb[PIPE_BUF];
  int lbcnt;

  unsigned long fz_high;        /* Frozen cycle high */
  unsigned long fz_low;         /* Frozen cycle low */

#ifdef TRACK_PER_TID
  // vfd stuff
  fd_set tracked;
  vfd_t  vfd_tab[MAX_FDS];      /* File descriptors */
  vsel_t vsel_data;             /* select() data */
#endif

} thread_ctx_t;

// ##########################################################

void          lb_flush();
char         *lb_alloc_bytes(int bytes);
void          alloc_ctx();
thread_ctx_t *get_ctx();
void          free_ctx();

unsigned long long int get_tid();
pid_t get_pid();

#ifdef TRACK_PER_TID
fd_set *get_tracked();
vsel_t *get_vsel_data();
vfd_t  *get_vfd_tab(int i);
#endif

// ##########################################################

void LOGBIN0(int sc, int rv);
void LOGBINli(int sc, int rv, pthread_t _ptid, int ppid);
void LOGBINs(int sc, int rv, const char *s);
void LOGBINss(int sc, int rv, char *s1, char *s2);
void LOGBINis(int sc, int rv, int _i1, const char *s);
void LOGBINiis(int sc, int rv, int i1, int i2, const char *s);
void LOGBINiiis(int sc, int rv, int i1, int i2, int i3, const char *s);
void LOGBINiii(int sc, int rv, int i1, int i2, int i3);
void LOGBINii(int sc, int rv, int i1, int i2);
void LOGBINi(int sc, int rv, int i1);
void LOGBINfatal(const char *s);
void LOGBINia(int sc, int rv, int i1, const struct sockaddr *sa1);
void LOGBINiaa(int sc, int rv, int i1,
	       const struct sockaddr *sa1, const struct sockaddr *sa2);

void lb_freeze_current_time();
void lb_thaw_current_time();

// ##########################################################

#endif
