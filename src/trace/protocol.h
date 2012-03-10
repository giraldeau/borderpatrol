// Blackbird
//
// $Id: protocol.h,v 1.17 2009-03-18 17:19:02 ning Exp $
//
// protocol plugins
//

#ifndef PROTOCOL_H
#define PROTOCOL_H

#define DEBUG_PP 0

// ##########################################################

typedef enum pp_directions {
  PP_READ = 0,
  PP_WRITE
} pp_direction_t;

typedef struct pp_dispatcher {
  const char const *name;
  void          (*pp_init)(int s, void **data);
  void          (*pp_shutdown)(void **data);
  int           (*pp_read)(void *data, int s, void *buf,
                           int len);
  int           (*pp_write)(void *data, int s, const void *buf,
                            int len);
} pp_dispatcher_t;

pp_dispatcher_t *pp_id_bind(const struct sockaddr *);
pp_dispatcher_t *pp_id_connect(const struct sockaddr *);
pp_dispatcher_t *pp_id_exec(char *);

// ##########################################################

#endif

