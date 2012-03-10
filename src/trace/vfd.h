// Blackbird
//
// $Id: vfd.h,v 1.22 2007-06-26 20:20:59 ejk Exp $
//
// virtualized fd library
//

#ifndef VFD_H
#define VFD_H

#define MAX_FDS                 1024
#define MAX_SOCK_FN             256
#define VBUFSIZE                65536 //32768 //2048

enum vfd_types {
  VFD_NONE = 0,
  VFD_SOCK,
  VFD_PIPE,
  VFD_DUP,
  VFD_FILE
};

enum vfd_rwe {
  VFD_SEL_NONE = 0,
  VFD_SEL_READ,
  VFD_SEL_WRITE,
  VFD_SEL_EXCEPT
};

struct pp_dispatcher_t;

typedef struct vfd {
  int                   type;
  char                  rbuf[VBUFSIZE];
  int                   rbuflen, rbufofs, rlastlen, tosend, ppwantmore;
  char                  wbuf[VBUFSIZE];
  int                   wbuflen, wbufofs;
  int                   in_prog;

  // short                 poll_events, poll_revents;
  // pthread_mutex_t       poll_events_lock;

  pp_dispatcher_t       *pp_dispatcher;
  void                  *pp_data;
} vfd_t;

typedef struct vsel {
  int                   n, num;
  fd_set                read, write, except;
  pthread_mutex_t       lock;
} vsel_t;

#endif  // VFD_H
