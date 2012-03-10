// Blackbird
//
// $Id: protocol.c,v 1.75 2009-03-18 17:19:02 ning Exp $
//
// protocol plugins
//

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <assert.h>
#include "config.h"
#include "protocol.h"
#include "vfd.h"
#include "wrap.h"
#include "getservice.h"

extern int bb_disable_pp;

// ##########################################################
// README

// Implement SYS_PP_MSG_SEND
//           SYS_PP_MSG_RECV
//           SYS_PP_INIT
//           SYS_PP_REQ_BEGIN

// If REQ_BEGIN happens at the same time as MSG_RECV,
//   put REQ_BEGIN first.

// ##########################################################
// helpers

// ##########################################################
//
// perlp: Single-Line Protocol
//
// ##########################################################

typedef struct pp_perlp_state {
  int   c2s_ctr;
  int   s2c_ctr;
} pp_perlp_state_t;


// server side
void pp_perlps_init(int s, void **data) {
  pp_perlp_state_t *state;
  state = (pp_perlp_state_t *)malloc(sizeof(pp_perlp_state_t));
  state->c2s_ctr = 0;
  state->s2c_ctr = 1;
  *data = state;

  LOGBINiis(SYS_PP_INIT,0,PP_PERLS_ID,s,"perlps");
}

void pp_perlps_shutdown(void **data) {
  if (*data) {
    free(*data);
    *data = NULL;
  }
  LOGBINi(SYS_PP_SHUTDOWN,0,PP_PERLS_ID);
}

int pp_perlps_read(void *data, int s, void *buf, int len) {
  pp_perlp_state_t *state = (pp_perlp_state_t *)data;
  //char tmp[1024];
  //strncpy(tmp,(char *)buf,1024);
  LOGBINiiis(SYS_PP_MSG_RECV,0,PP_PERLS_ID, s, state->s2c_ctr++,"perlps");
  return len;
}

int pp_perlps_write(void *data, int s, const void *buf, int len){
  pp_perlp_state_t *state = (pp_perlp_state_t *)data;
  //char tmp[1024];
  //strncpy(tmp,(char *)buf,1024);
  LOGBINiiis(SYS_PP_MSG_SEND,0,PP_PERLS_ID, s, state->s2c_ctr++,"perlps");
  return len;
}

// client side
void pp_perlpc_init(int s, void **data) { }
void pp_perlpc_shutdown(void **data) { }
int pp_perlpc_read(void *data, int s, void *buf, int len) { return len + 1; }
int pp_perlpc_write(void *data, int s, const void *buf,int len) { return len + 1; }

// ##########################################################
//
// HTTP/1.*
//
// GET /test.html HTTP/1.0
// User-Agent: Wget/1.9.1
// Host: localhost
// Accept: */*
// Connection: Keep-Alive
//
// ##########################################################

#define HTTPC_INIT                      0
#define HTTPC_GOTG                      1
#define HTTPC_GOTE                      2
#define HTTPC_GOTT                      3
#define HTTPC_GOTT_                     4
#define HTTPC_GOT_URL                   5
#define HTTPC_GOT_URL_H                 6
#define HTTPC_GOT_URL_HT                7
#define HTTPC_GOT_URL_HTT               8
#define HTTPC_GOT_URL_HTTP              9
#define HTTPC_GOT_URL_HTTP_             10
#define HTTPC_GOT_URL_HTTP_1            11
#define HTTPC_GOT_URL_HTTP_1_           12
#define HTTPC_GOT_VER                   13
#define HTTPC_GOTR1                     14
#define HTTPC_GOTN1                     15
#define HTTPC_GOTR2                     16
#define HTTPC_GOTN2                     17

#define HTTPS_INIT                      0
#define HTTPS_GOTC                      1
#define HTTPS_GOTCo                     2
#define HTTPS_GOTCon                    3
#define HTTPS_GOTCont                   4
#define HTTPS_GOTConte                  5
#define HTTPS_GOTConten                 6
#define HTTPS_GOTContent                7
#define HTTPS_GOTContent_               8
#define HTTPS_GOTContent_L              9
#define HTTPS_GOTContent_Le             10
#define HTTPS_GOTContent_Len            11
#define HTTPS_GOTContent_Leng           12
#define HTTPS_GOTContent_Lengt          13
#define HTTPS_GOTContent_Length         14
#define HTTPS_GOTContent_Length_        15
#define HTTPS_GOTContent_Length__       16
#define HTTPS_GOT_CL_R1                 17
#define HTTPS_GOT_CL_N1                 18
#define HTTPS_GOT_CL_RN                 19
#define HTTPS_GOTR1                     20
#define HTTPS_GOTN1                     21
#define HTTPS_GOTR2                     22
#define HTTPS_SKIP_CONT                 23

#define HTTPS_MAX_URL                   1024
#define HTTPS_MAX_CLLEN                 16

typedef struct pp_http_state_read {
  int done;
  int awaitHeader;
  int awaitCL;
  int version;
  int seqno;
  int toread;
} pp_http_state_read_t;

typedef struct pp_http_state_write {
  int done;
  int awaitStatus;
  int toread;
  int seqno;
} pp_http_state_write_t;

typedef struct pp_http_state {
  pp_http_state_read_t read;
  pp_http_state_write_t write;
} pp_http_state_t;

// ##########################################################
// client side

void pp_httpc_init(int s, void **data) { }
void pp_httpc_shutdown(void **data) { }
int pp_httpc_read(void *data, int s, void *buf, int len) { return len; }
int pp_httpc_write(void *data, int s, const void *buf, int len) { return len; }

// ##########################################################
// server side

void pp_https_init(int s, void **data)
{
  pp_http_state_t *state;
  state = (pp_http_state_t *)malloc(sizeof(pp_http_state_t));
  state->read.done = state->read.awaitCL = state->read.toread = state->read.seqno = 0;
  state->read.version = -1; state->read.awaitHeader = 1;
  state->write.done = state->write.toread = state->write.seqno = 0;
  state->write.awaitStatus = 1;
  *data = state;
  LOGBINiis(SYS_PP_INIT,0,PP_HTTPS_ID,s,"https");
}

void pp_https_shutdown(void **data)
{
  if (*data) {
    free(*data);
    *data = NULL;
  }
  LOGBINis(SYS_PP_SHUTDOWN,0,PP_HTTPS_ID,"https");
}

int pp_https_read(void *data, int s, void *buf, int len)
{
  char *cbuf = (char *)buf;
  pp_http_state_t *stt = (pp_http_state_t *)data;
  pp_http_state_read_t *state = &(stt->read);

  if(state->done)
    return len;

  if(state->awaitHeader) {
    char *end = strstr(cbuf,"\r\n\r\n");
    if(!end) return -1;
    state->awaitHeader = 0;

    LOGBINiiis(SYS_PP_MSG_RECV,0,PP_HTTPS_ID,s, state->seqno++, "https");

    // strtok modifies the buffer in place
    char *tmp = malloc(len+1);
    strncpy(tmp,cbuf,len);
    LOGBINs(SYS_HTTPS_READ,len,tmp);

    char *meth = strtok(tmp," \r");
    LOGBINss(SYS_PP_PROP,0,"METHOD", (meth?meth:"null"));

    char *url = strtok(NULL," \r");
    //fprintf(stderr,"URL: %s\n", url);
    LOGBINss(SYS_PP_PROP,0,"URL", (url?url:"null"));

    char *version = strtok(NULL," \r");
    LOGBINss(SYS_PP_PROP,0,"VERSION", (version?version:"null"));

    //fprintf(stderr,"-----------\nHEAD=\n%s\nVERSION=%s\n--\n", cbuf, version);
    if(strstr(version,"HTTP/1.0")) {
      state->version = 0;
      state->done = 1;
    } else if(strstr(version,"HTTP/1.1")) {
      state->version = 1;
      state->awaitHeader = 1;
    } else {
      lb_flush();
      assert(0);
    }
    free(tmp);
    return (end - cbuf + 4);
  }

  assert(state->toread > 0);

  /* Shouldn't reach here */
  return -1;
}

//HTTP/1.1 200 OK
//Date: Fri, 16 Feb 2007 01:53:29 GMT
//Server: Apache/1.3.37 (Unix) mod_fastcgi/2.4.2
//Content-Length: 20087
//Set-Cookie: tg-visit=af057ea27d6ab8946679df682afa3ae50ff0aa34; Path=/;
//Content-Type: text/html; charset=utf-8

//HTTP/1.1 404 Not Found
//Content-Length: 2814

int pp_https_write(void *data, int s, const void *buf,
                       int len)
{
  char *cbuf = (char *)buf;
  pp_http_state_t *stt = (pp_http_state_t *)data;
  pp_http_state_write_t *state = &(stt->write);

  //fprintf(stderr,"write: r=%d, d=%d, a=%d\n",
  //state->toread, state->done, state->awaitStatus);

  if(state->done)
    return len;

  if(state->toread) {
    int consumed = (len > state->toread ? state->toread : len);
    state->toread -= consumed;
    if (state->toread == 0)
      state->awaitStatus = 1;
    return consumed;
  }

  if(state->awaitStatus) {
    char tmp[1024];
    char *end = strstr(cbuf,"\r\n\r\n");
    if(!end) return -1;
    state->awaitStatus = 0;

    // strtok modifies the buffer in place
    strncpy(tmp,cbuf,1024);

    char *version = strtok(tmp," \r");
    LOGBINss(SYS_PP_PROP,0,"VERSION", (version?version:"null"));       // todo -- compare version with clen

    char *status = strtok(NULL," \r");
    LOGBINss(SYS_PP_PROP,0,"STATUS", (status?status:"null"));

    // get lines until we get the Content-Type line
    char *line;
    //LOGBIN0(SYS_DEBUGA,1);
    while((line = strtok(NULL,"\r"))) {
      if(strstr(line,"Content-Length: ")) {
        LOGBINss(SYS_PP_PROP,0,"CLEN", &line[17]);
        state->toread = atoi(&line[17]);
        break;
      } else if (strstr(line,"Connection: close")) {
        state->done = 1;
        break;
      }
    }
    LOGBINiiis(SYS_PP_MSG_SEND,0,PP_HTTPS_ID,s, state->seqno++, "https");

    return (end - cbuf + 4);
  }

  return len;
}

// ##########################################################
//
// POSTGRES
//
// http://developer.postgresql.org/docs/postgres/protocol-overview.html
// http://www.postgresql.org/docs/8.1/interactive/protocol.html
//
// All communication is through a stream of messages. The first byte
// of a message identifies the message type, and the next four bytes
// specify the length of the rest of the message (this length count
// includes itself, but not the message-type byte). The remaining
// contents of the message are determined by the message type. For
// historical reasons, the very first message sent by the client (the
// startup message) has no initial message-type byte.
//
// /research/csrgrant/3t/src/postgresql-8.1.3/src/backend/libpq/pqcomm.c get_message()
//
// ##########################################################

typedef struct pp_psql_state_side {
  int gotStartup;
  char type;
  int gotHeader;
  unsigned int todo;
  int id;
} pp_psql_state_side_t;

typedef struct pp_psql_state {
  pp_psql_state_side_t read;
  pp_psql_state_side_t write;
} pp_psql_state_t;

// /research/source/postgresql-8.1.4/src/interfaces/libpq/fe-misc.c
void get_int(unsigned int *result, const void *buf) {
  unsigned int tmp; //uint32

  memcpy((void *)&tmp, buf, 4);
  if(DEBUG_PP) LOGBINis(SYS_PP_DATA,0,(int)ntohl(tmp),"get_int");
  *result = ntohl(tmp);
}

// ##########################################################

int speak_psql(pp_psql_state_side_t *state,int s,const void *buf,
               unsigned int len, int role, pp_direction_t dir) {
  char tmp[68];

  // !!!!!!! REALLY BACK TEMPORARY HACK TO FIX TIMING ISSUES !!!!!!!!
  if(dir == PP_READ) usleep(10);

  // handle the case where we're spinning, waiting for more data
  if(state->todo > 0) {
    if (state->type == 'Q') {
      if (len < state->todo) return -1;
      LOGBINss(SYS_PP_PROP,0,"QUERY",(char *)buf);
    }

    int used = (len > state->todo ? state->todo : len);
    state->todo -= used;
    if(state->todo == 0) {
      // todo : MSG_SEND/RECV here
      state->gotHeader = 0;
    }
    if(DEBUG_PP) {
      snprintf(tmp,67,"psql: used=%d todo=%d",used,state->todo);
      LOGBINs(SYS_PP_DATA,0,tmp);
    }
    return used;
  }

  // PSQL first involves a startup message with no 'type' field
  if(! state->gotStartup) {
    if (len < 8)
      return -1;

    state->gotStartup = 1;
    get_int(&(state->todo),buf);
    state->todo -= 8;

    unsigned int prot;
    get_int(&prot,buf+4);
/*     assert(prot == 0x30000); */

    if(DEBUG_PP) {
      sprintf(tmp,"PROTOCOL: %x",prot);
      LOGBINs(SYS_PP_DATA,0,tmp);
      snprintf(tmp,67,"psql: startup todo=%d. ver=%d",state->todo,prot);
      LOGBINs(SYS_PP_DATA,0,tmp);
    }
    return 8;
  }

  // normally, message headers include a 'type' field:
  if(! state->gotHeader) {
    if (len < 5)
      return -1;

    char *c = (char *)buf;
    state->type = *c;
    state->gotHeader = 1;
    get_int(&(state->todo),buf+1);
    state->todo -= 4;
    if(1) {
      snprintf(tmp,67,"psql: type=%c len=%d todo=%d",state->type,state->todo+4,state->todo);
      LOGBINs(SYS_PP_DATA,0,tmp);
    }

    switch (state->type) {
    case 'Q': //Query
      if(role == PP_PSQLC_ID && dir == PP_WRITE)
	LOGBINiiis(SYS_PP_MSG_SEND,0,PP_PSQLC_ID,s,state->id++,"psqlc");
      else if(role == PP_PSQLS_ID && dir == PP_READ)
	LOGBINiiis(SYS_PP_MSG_RECV,0,PP_PSQLS_ID,s,state->id++,"psqls");
      break;
      //case 'C': //CommandComplete
    case 'Z': //ReadyForQuery
      if(role == PP_PSQLC_ID && dir == PP_READ)
	LOGBINiiis(SYS_PP_MSG_RECV,0,PP_PSQLC_ID,s,state->id++,"psqlc");
      else if(role == PP_PSQLS_ID && dir == PP_WRITE)
	LOGBINiiis(SYS_PP_MSG_SEND,0,PP_PSQLS_ID,s,state->id++,"psqls");
      break;
    }
    return 5;
  }

  assert(1); // should never get here

  return -1;
}

// ##########################################################

void pp_psql_common_init(pp_psql_state_t *state) {
  state->read.gotStartup  = state->read.gotHeader  = state->read.todo = 0;
  state->write.gotStartup = state->write.gotHeader = state->write.todo = 0;
  state->write.type = state->read.type = '\0';
  state->read.id = state->write.id = 0;
}


// ##########################################################
// client side

void pp_psqlc_init(int s, void **data) {
  pp_psql_state_t *state = (pp_psql_state_t *)malloc(sizeof(pp_psql_state_t));
  pp_psql_common_init(state);
  state->read.gotStartup = 1; // client doesn't read a Startup
  *data = state;
  LOGBINiis(SYS_PP_INIT,0,PP_PSQLC_ID,s,"psqlc");
}

void pp_psqlc_shutdown(void **data) {
  if (*data) {
    free(*data);
    *data = NULL;
  }
  LOGBINs(SYS_PP_SHUTDOWN,0,"psqlc");
}

int pp_psqlc_read(void *data, int s, void *buf, int len){
  pp_psql_state_side_t *state = &(((pp_psql_state_t *)data)->read);
  return speak_psql(state,s,buf,len,PP_PSQLC_ID,PP_READ);
}


int pp_psqlc_write(void *data, int s, const void *buf, int len) {
  pp_psql_state_side_t *state = &(((pp_psql_state_t *)data)->write);
  return speak_psql(state,s,buf,len,PP_PSQLC_ID,PP_WRITE);
}


// ##########################################################
// server side

void pp_psqls_init(int s, void **data) {
  pp_psql_state_t *state = (pp_psql_state_t *)malloc(sizeof(pp_psql_state_t));
  pp_psql_common_init(state);
  state->write.gotStartup = 1; // server doesn't write a Startup
  *data = state;
  LOGBINiis(SYS_PP_INIT,0,PP_PSQLS_ID,s,"psqls");
}

void pp_psqls_shutdown(void **data) {
  if (*data) {
    free(*data);
    *data = NULL;
  }
  LOGBINs(SYS_PP_SHUTDOWN,0,"psqls");
}

int pp_psqls_read(void *data, int s, void *buf, int len){
  pp_psql_state_side_t *state = &(((pp_psql_state_t *)data)->read);
  return speak_psql(state,s,buf,len,PP_PSQLS_ID,PP_READ);
}

int pp_psqls_write(void *data, int s, const void *buf, int len) {
  pp_psql_state_side_t *state = &(((pp_psql_state_t *)data)->write);
  return speak_psql(state,s,buf,len,PP_PSQLS_ID,PP_WRITE);
}

/**
 * MYSQL
 *
 * http://www.redferni.uklinux.net/mysql/MySQL-Protocol.html
 *
 * All communication is through a stream of messages. The first three bytes of a
 * message specify the message length (this length count does not include itself
 * and the one-byte ID following it. Least significant byte first), and the next
 * byte identifies the message ID.
 */

typedef struct pp_mysql_state_side {
  unsigned int todo;
  int id;
} pp_mysql_state_side_t;

typedef struct pp_mysql_state {
  pp_mysql_state_side_t read;
  pp_mysql_state_side_t write;
} pp_mysql_state_t;

void get_mysql_size(unsigned int *result, const void *buf) {
	char *tmp = (char *)malloc(3);

  memcpy((void *)tmp, buf, 3);
  *result = (unsigned int)(tmp[0] | (tmp[1] << 8) | (tmp[2] << 16));

  if(DEBUG_PP) LOGBINis(SYS_PP_DATA, 0, *result, "get_mysql_size");
}

int speak_mysql(pp_mysql_state_side_t *state,int s,const void *buf,
                unsigned int len, int role, pp_direction_t dir) {
  char tmp[68];

  // !!!!!!! REALLY BACK TEMPORARY HACK TO FIX TIMING ISSUES !!!!!!!!
  if(dir == PP_READ) usleep(10);

  /* Handle the case where we're spinning, waiting for more data */
  if(state->todo > 0) {
    int used = (len > state->todo ? state->todo : len);
    state->todo -= used;

    if(DEBUG_PP) {
      snprintf(tmp,67,"mysql: used=%d todo=%d",used,state->todo);
      LOGBINs(SYS_PP_DATA,0,tmp);
    }

    return used;
  }

  if (len < 4) {
    return -1;
  }

  /* Get the length of the packet */
  get_mysql_size(&(state->todo), buf);
  /* Get the packet number */
  memcpy((void *)&(state->id), (char *)buf + 3, 1);

  snprintf(tmp, 67, "mysql: id=%d todo=%d", state->id, state->todo);
  LOGBINs(SYS_PP_DATA,0,tmp);

  switch (role) {
  case PP_MYSQLC_ID:            /* Client */
    if (dir == PP_WRITE) {
      LOGBINiiis(SYS_PP_MSG_SEND, 0, PP_MYSQLC_ID, s, state->id, "mysqlc");
    } else if (dir == PP_READ) {
      LOGBINiiis(SYS_PP_MSG_RECV, 0, PP_MYSQLC_ID, s, state->id, "mysqlc");
    }

    break;
  case PP_MYSQLS_ID:            /* Server */
    if (dir == PP_WRITE) {
      LOGBINiiis(SYS_PP_MSG_SEND, 0, PP_MYSQLS_ID, s, state->id, "mysqls");
    } else if (dir == PP_READ) {
      LOGBINiiis(SYS_PP_MSG_RECV, 0, PP_MYSQLS_ID, s, state->id, "mysqls");
    }

    break;
  }

  return 4;
}

void pp_mysql_common_init(pp_mysql_state_t *state) {
  state->read.todo = 0;
  state->write.todo = 0;
  state->read.id = state->write.id = 0;
}

/* client side */

void pp_mysqlc_init(int s, void **data) {
  pp_mysql_state_t *state = (pp_mysql_state_t *)malloc(sizeof(pp_mysql_state_t));
  pp_mysql_common_init(state);
  *data = state;
  LOGBINiis(SYS_PP_INIT,0,PP_MYSQLC_ID,s,"mysqlc");
}

void pp_mysqlc_shutdown(void **data) {
  if (*data) {
    free(*data);
    *data = NULL;
  }
  LOGBINs(SYS_PP_SHUTDOWN,0,"mysqlc");
}

int pp_mysqlc_read(void *data, int s, void *buf, int len){
  pp_mysql_state_side_t *state = &(((pp_mysql_state_t *)data)->read);
  return speak_mysql(state,s,buf,len,PP_MYSQLC_ID,PP_READ);
}


int pp_mysqlc_write(void *data, int s, const void *buf, int len) {
  pp_mysql_state_side_t *state = &(((pp_mysql_state_t *)data)->write);
  return speak_mysql(state,s,buf,len,PP_MYSQLC_ID,PP_WRITE);
}

/* server side */

void pp_mysqls_init(int s, void **data) {
  pp_mysql_state_t *state = (pp_mysql_state_t *)malloc(sizeof(pp_mysql_state_t));
  pp_mysql_common_init(state);
  *data = state;
  LOGBINiis(SYS_PP_INIT,0,PP_MYSQLS_ID,s,"mysqls");
}

void pp_mysqls_shutdown(void **data) {
  if (*data) {
    free(*data);
    *data = NULL;
  }
  LOGBINs(SYS_PP_SHUTDOWN,0,"mysqls");
}

int pp_mysqls_read(void *data, int s, void *buf, int len){
  pp_mysql_state_side_t *state = &(((pp_mysql_state_t *)data)->read);
  return speak_mysql(state,s,buf,len,PP_MYSQLS_ID,PP_READ);
}

int pp_mysqls_write(void *data, int s, const void *buf, int len) {
  pp_mysql_state_side_t *state = &(((pp_mysql_state_t *)data)->write);
  return speak_mysql(state,s,buf,len,PP_MYSQLS_ID,PP_WRITE);
}


// ##########################################################
//
// X
//
// http://www.msu.edu/~huntharo/xwin/docs/xwindows/PROTO.pdf
//
// Communication consists of four message types
//  Requests to the server
//  Responses from the server
//  Events from the server
//  Errors from the server
//
// ##########################################################

typedef struct pp_x_state {
  int success;                  /* Do we have the first 8 bytes? */
  int setup;                    /* How many bytes does setup need to read? */
  int recvid;
  int sendid;
} pp_x_state_t;

void pp_xc_init(int s, void **data) {
  pp_x_state_t* state = (pp_x_state_t*)malloc(sizeof(pp_x_state_t));
  state->success = 0;
  state->sendid = state->recvid = 0;
  *data = state;
  LOGBINiis(SYS_PP_INIT,0,PP_XC_ID,s,"xc");
}

void pp_xc_shutdown(void **data) {
  if (*data) {
    free(*data);
    *data = NULL;
  }
  LOGBINs(SYS_PP_SHUTDOWN,0,"xc");
}


static int CARD16(unsigned char* c) {
  return (c[1] << 8) | c[0];
}

int pp_xc_read(void *data, int s, void *buf, int len) {
  unsigned char *cbuf = buf;
  pp_x_state_t* state = (pp_x_state_t*)data;

  if (!state->success) {
    assert(cbuf[0] == 1);       /* Success */
    if (len < 8)
      return -1;
    state->success = 1;
    state->setup = 4*CARD16(&cbuf[6]);
    LOGBINs(SYS_PP_REQ_BEGIN,state->setup,"xc_read");
    LOGBINiiis(SYS_PP_MSG_RECV,0,PP_XC_ID,s, state->recvid++,"xc");
    return 8;
  }

  if (state->setup > 0) {
    if (len < state->setup)
      return -1;
    int ret = state->setup;
    state->setup = 0;
    return ret;
  }

  if (len < 32)
    return -1;
  return 32;
}

int pp_xc_write(void *data, int s, const void *buf, int len) {
  return len;
}


// ##########################################################
//
// FASTCGI
//
// http://www.fastcgi.com/devkit/doc/fcgi-spec.html#S3.3
// http://www.fastcgi.com/cvs/fcgi2/libfcgi/fcgiapp.c
//   Search for: FCGX_Stream_Data (interpret the stream)
// http://www.fastcgi.com/cvs/fcgi2/include/fastcgi.h
// Search for: FCGI_Header (marshall into a stream)
//
// ##########################################################


// *********************************************************
// Protocol Definitions

#define FCGI_STATE_INIT -1

#define FCGI_BEGIN_REQUEST       1
#define FCGI_ABORT_REQUEST       2
#define FCGI_END_REQUEST         3
#define FCGI_PARAMS              4
#define FCGI_STDIN               5
#define FCGI_STDOUT              6
#define FCGI_STDERR              7
#define FCGI_DATA                8
#define FCGI_GET_VALUES          9
#define FCGI_GET_VALUES_RESULT  10
#define FCGI_UNKNOWN_TYPE       11
#define FCGI_MAXTYPE (FCGI_UNKNOWN_TYPE)

#define FCGI_HEADER_LEN      8

char *type_of_record(int num){
  switch (num) {
  case 1: return "FCGI_BEGIN_REQUEST";
  case 2: return "FCGI_ABORT_REQUEST";
  case 3: return "FCGI_END_REQUEST";
  case 4: return "FCGI_PARAMS";
  case 5: return "FCGI_STDIN";
  case 6: return "FCGI_STDOUT";
  case 7: return "FCGI_STDERR";
  case 8: return "FCGI_DATA";
  case 9: return "FCGI_GET_VALUES";
  case 10: return "FCGI_GET_VALUES_RESULT";
  default: return "FCGI_UNKNOWN_TYPE";
  }
}

typedef struct {
  unsigned char version;
  unsigned char type;
  unsigned char requestIdB1;
  unsigned char requestIdB0;
  unsigned char contentLengthB1;
  unsigned char contentLengthB0;
  unsigned char paddingLength;
  unsigned char reserved;
  //unsigned char contentData[contentLength];
  //unsigned char paddingData[paddingLength];
} FCGI_REC_t;

// *********************************************************
// FCGI State

typedef struct pp_fcgi_state {
  int success;                  /* Do we have the first 4 bytes? */
  int conlen;                   /* How many bytes do we need to read? */
  int padlen;                   /* How many bytes do we need to read? */
  char *stream_type;
  int reqId;
  int done;
} pp_fcgi_state_t;

typedef struct pp_fcgi_state_wrap {
  pp_fcgi_state_t read;
  pp_fcgi_state_t write;
} pp_fcgi_state_wrap_t;

pp_fcgi_state_wrap_t *pp_fcgi_init_help() {
  pp_fcgi_state_wrap_t *p =
    (pp_fcgi_state_wrap_t *)malloc(sizeof(pp_fcgi_state_wrap_t));
  p->read.success = 0;
  p->read.conlen  = 0;
  p->read.padlen  = 0;
  p->read.reqId   = -1;
  p->read.stream_type = NULL;
  p->read.done = 0;
  p->write.success = 0;
  p->write.conlen  = 0;
  p->write.padlen  = 0;
  p->write.reqId   = -1;
  p->write.stream_type = NULL;
  p->write.done = 0;
  return p;
}

// *********************************************************
// FCGI Helpers


static void parse_sockaddr(const struct sockaddr *addr, int addrlen,
                           char *name, int len)
{
  if (addr->sa_family == AF_INET  ||  addr->sa_family == AF_INET6) {
    char host[INET_ADDRSTRLEN+1];
    host[0] = '\0';
    if (((struct sockaddr_in *)addr)->sin_addr.s_addr == INADDR_ANY)
      strcpy(host, "127.0.0.1");
    else
      inet_ntop(addr->sa_family,
                &((struct sockaddr_in *)addr)->sin_addr,
                host, INET_ADDRSTRLEN);
    int port = ntohs(((struct sockaddr_in *)addr)->sin_port);
    snprintf(name, len, "%s:%d", host, port);
  }
  else
    strncpy(name, "unset", len);
}


int speak_fcgi(pp_fcgi_state_t *state, int s, unsigned char *cbuf, int len,
	       int role, pp_direction_t d){

  if(state->done) return len;

  if (!state->success) {
    if (len < FCGI_HEADER_LEN)
      return -1;
    state->success = 1;

    FCGI_REC_t *f = (FCGI_REC_t *)cbuf;
    state->reqId  = ((int)(f->requestIdB1 << 8)) + (int)f->requestIdB0;
    state->conlen = (f->contentLengthB1 << 8) + f->contentLengthB0;
    state->padlen = (int)f->paddingLength;

    char *sarg = (role == PP_FCGIS_ID ? "fcgis" : "fcgic");

    // if there's no content, we want to get the next header
    if (state->conlen + state->padlen == 0)
      state->success = 0;

    //if(d == PP_READ) {
      /*      char lhostport[512], rhostport[512];
      char ctmp[MAX_LOG_LINE];
      struct sockaddr laddr;
      struct sockaddr raddr;
      strcpy(lhostport, "other");
      strcpy(rhostport, "other");
      unsigned int raddrlen = sizeof (struct sockaddr);
      if (getpeername(s, &raddr, &raddrlen) != -1)
	parse_sockaddr(&raddr, raddrlen, rhostport, 512);
      unsigned int laddrlen = sizeof (struct sockaddr);
      if (getsockname(s, &laddr, &laddrlen) != -1)
	parse_sockaddr(&laddr, laddrlen, lhostport, 512);
      strcpy(ctmp,rhostport);
      strcat(ctmp,"->");
      strcat(ctmp,lhostport);
      LOGBINis(SYS_ACCEPT,s,s,ctmp); */
      // \--- ejk

      //LOGBINiiis(SYS_PP_MSG_RECV,(int)f->type,role,s,state->reqId,sarg);
    //}
    //else
    //LOGBINiiis(SYS_PP_MSG_SEND,(int)f->type,role,s,state->reqId,sarg);

    if((int)f->type == FCGI_PARAMS)
      state->stream_type = "PARAMS:";

    LOGBINs(SYS_PP_FCGI_HMM,0,type_of_record((int)f->type));

    switch ((int)f->type) {
    /* the web server sends this message to an FCGI server to start things off */
    case FCGI_BEGIN_REQUEST:
      if(d == PP_READ)
	LOGBINiiis(SYS_PP_MSG_RECV,(int)f->type,role,s,state->reqId,sarg);
      else
	LOGBINiiis(SYS_PP_MSG_SEND,(int)f->type,role,s,state->reqId,sarg);
      break;

    /* after a FCGI server is done, they send this message back to the web server */
    case FCGI_END_REQUEST:
      if(d == PP_READ)
	LOGBINiiis(SYS_PP_MSG_RECV,(int)f->type,role,s,state->reqId,sarg);
      else
	LOGBINiiis(SYS_PP_MSG_SEND,(int)f->type,role,s,state->reqId,sarg);
      break;

      // Zeus does something dumb: it reads on STDOUT and considers that the end
    case FCGI_STDOUT:
      if(d == PP_READ)
	LOGBINiiis(SYS_PP_MSG_RECV,(int)f->type,role,s,state->reqId,sarg);
      else
	LOGBINiiis(SYS_PP_MSG_SEND,(int)f->type,role,s,state->reqId,sarg);
      state->done = 1;
      break;

    case FCGI_PARAMS:
      state->stream_type = "PARAMS:";
      break;

    default:
      break;
    }

    return FCGI_HEADER_LEN;
  }

  int toread = (state->conlen) + (state->padlen);

  // should only get here if 'success' and there's a body to read
  assert(toread > 0);

  // This code is bad: it will cause us to buffer up a big message
  // and when we're ready to pass it to the caller, it may be
  // bigger than the caller's allocated buffer.
  // *** if (len < toread) return -1; ***

  if(len >= toread) {
    // end this request, come back with the balance
    // TODO : fix this. right now we don't buffer up the cbuf data,
    //   so when this fires, we'll only see the end
    char tmp[1024];
    if(state->stream_type) {
      //strcpy(tmp,state->stream_type);
      int l = state->conlen-2;
      strncpy(tmp,&cbuf[2],(l>len ? len : l));
      LOGBINss(SYS_PP_PROP,0,state->stream_type,tmp);
      // PP_FCGIS_ID, s, state->reqId, tmp);
      state->stream_type = NULL;
    }
    state->success = 0;
    state->conlen  = -1;
    state->padlen  = -1;
    state->reqId   = -1;
    return toread; // only used 'toread' of the bytes
  } else {
    toread -= len;
    return len;
  }

}

// *********************************************************
// FCGI Server

void pp_fcgis_init(int s, void **data)
{
  *data = pp_fcgi_init_help();
  LOGBINiis(SYS_PP_INIT,0,PP_FCGIS_ID,s,"fcgis");
}

void pp_fcgis_shutdown(void **data) {
  if (*data) {
    free(*data);
    *data = NULL;
  }
  LOGBINs(SYS_PP_SHUTDOWN,0,"fcgis");
}

int pp_fcgis_read(void *data, int s, void *buf, int len){
  unsigned char *cbuf = buf;
  pp_fcgi_state_t *state = &(((pp_fcgi_state_wrap_t *)data)->read);
  return speak_fcgi(state, s, cbuf, len, PP_FCGIS_ID, PP_READ);
}

int pp_fcgis_write(void *data, int s, const void *buf, int len) {
  unsigned char *cbuf = (unsigned char *)buf;
  pp_fcgi_state_t *state = &(((pp_fcgi_state_wrap_t *)data)->write);
  return speak_fcgi(state, s, cbuf, len, PP_FCGIS_ID, PP_WRITE);
}

// *********************************************************
// FCGI Client

void pp_fcgic_init(int s, void **data)
{
  *data = pp_fcgi_init_help();
  LOGBINiis(SYS_PP_INIT,0,PP_FCGIC_ID,s,"fcgic");
}

void pp_fcgic_shutdown(void **data) {
  if (*data) {
    free(*data);
    *data = NULL;
  }
  LOGBINs(SYS_PP_SHUTDOWN,0,"fcgic");
}

int pp_fcgic_read(void *data, int s, void *buf, int len){
  unsigned char *cbuf = buf;
  pp_fcgi_state_t *state = &(((pp_fcgi_state_wrap_t *)data)->read);
  return speak_fcgi(state, s, cbuf, len, PP_FCGIC_ID, PP_READ);
}

int pp_fcgic_write(void *data, int s, const void *buf, int len) {
  unsigned char *cbuf = (unsigned char *)buf;
  pp_fcgi_state_t *state = &(((pp_fcgi_state_wrap_t *)data)->write);
  return speak_fcgi(state, s, cbuf, len, PP_FCGIC_ID, PP_WRITE);
}

// *********************************************************
// Trivial Client

void pp_trivial_init(int s, void **data) {
  *data = NULL;
}

void pp_trivial_shutdown(void **data) {
  *data = NULL;
}

int pp_trivial_read(void *data, int s, void *buf, int len){
  return len;
}

int pp_trivial_write(void *data, int s, const void *buf, int len) {
  return len;
}

// ##########################################################
// protocol dispatcher structures

pp_dispatcher_t pp_perlpc = {
  "perlp",
  pp_perlpc_init,
  pp_perlpc_shutdown,
  pp_perlpc_read,
  pp_perlpc_write
};

pp_dispatcher_t pp_perlps = {
  "perlp",
  pp_perlps_init,
  pp_perlps_shutdown,
  pp_perlps_read,
  pp_perlps_write
};

pp_dispatcher_t pp_httpc = {
  "http",
  pp_httpc_init,
  pp_httpc_shutdown,
  pp_httpc_read,
  pp_httpc_write
};

pp_dispatcher_t pp_https = {
  "http",
  pp_https_init,
  pp_https_shutdown,
  pp_https_read,
  pp_https_write
};

pp_dispatcher_t pp_xc = {
  "x11",
  pp_xc_init,
  pp_xc_shutdown,
  pp_xc_read,
  pp_xc_write
};

pp_dispatcher_t pp_psqlc = {
  "postgresql",
  pp_psqlc_init,
  pp_psqlc_shutdown,
  pp_psqlc_read,
  pp_psqlc_write
};

pp_dispatcher_t pp_psqls = {
  "postgresql",
  pp_psqls_init,
  pp_psqls_shutdown,
  pp_psqls_read,
  pp_psqls_write
};

pp_dispatcher_t pp_mysqlc = {
  "mysql",
  pp_mysqlc_init,
  pp_mysqlc_shutdown,
  pp_mysqlc_read,
  pp_mysqlc_write
};

pp_dispatcher_t pp_mysqls = {
  "mysql",
  pp_mysqls_init,
  pp_mysqls_shutdown,
  pp_mysqls_read,
  pp_mysqls_write
};

pp_dispatcher_t pp_fcgis = {
  "fcgi",
  pp_fcgis_init,
  pp_fcgis_shutdown,
  pp_fcgis_read,
  pp_fcgis_write
};

pp_dispatcher_t pp_fcgic = {
  "fcgi",
  pp_fcgic_init,
  pp_fcgic_shutdown,
  pp_fcgic_read,
  pp_fcgic_write
};

pp_dispatcher_t pp_trivial = {
  "trivial",
  pp_trivial_init,
  pp_trivial_shutdown,
  pp_trivial_read,
  pp_trivial_write
};

// ##########################################################
// protocol identification

/* Return 1 if the ports match, otherwise return 0. */
int is_protocol_port(const char const *protocol_name, int port) {
  int protocol_port = getportbyname(protocol_name);

  if (protocol_port > 0) {
    return port == protocol_port;
  }

  return 0;
}

pp_dispatcher_t *pp_id_bind(const struct sockaddr *addr)
{
  if(bb_disable_pp) return NULL;
  if (addr->sa_family == AF_INET
      ||  addr->sa_family == AF_INET6) {
    int port = (int)ntohs(((struct sockaddr_in *)addr)->sin_port);
    if (is_protocol_port(pp_https.name, port)) {
      LOGBINs(SYS_PP_ID,0,"https");
      return &pp_https;
    }
    else if (is_protocol_port(pp_psqls.name, port)) {
      LOGBINs(SYS_PP_ID,0,"psqls");
      return &pp_psqls;
    }
    else if (is_protocol_port(pp_mysqls.name, port)) {
      LOGBINs(SYS_PP_ID,0,"mysqls");
      return &pp_mysqls;
    }
    else if (is_protocol_port(pp_perlps.name, port)) {
      LOGBINs(SYS_PP_ID,0,"perlps");
      return &pp_perlps;
    }
    else if (is_protocol_port(pp_fcgis.name, port)) {
      LOGBINs(SYS_PP_ID,0,"fcgis");
      return &pp_fcgis;
    }
    //LOGBIN0(SYS_PP_UNID1,port);
  }
  else if (addr->sa_family == AF_FILE) {
    struct sockaddr_un* sun = (struct sockaddr_un *)addr;
    if (strstr(sun->sun_path, "/var/run/postgl")) {
      LOGBINs(SYS_PP_ID,0,"psqls");
      return &pp_psqls;
    }
    if (strstr(sun->sun_path, "/var/run/mysql")) {
      LOGBINs(SYS_PP_ID,0,"mysqls");
      return &pp_mysqls;
    }
  }

  return NULL;
}

pp_dispatcher_t *pp_id_connect(const struct sockaddr *addr)
{
  if(bb_disable_pp) return NULL;
  if (addr->sa_family == AF_INET
      ||  addr->sa_family == AF_INET6) {
    int port = ntohs(((struct sockaddr_in *)addr)->sin_port);
    if (is_protocol_port(pp_httpc.name, port)) {
      LOGBINs(SYS_PP_ID,0,"httpc");
      return &pp_httpc;
    }
    else if (is_protocol_port(pp_psqlc.name, port)) {
      LOGBINs(SYS_PP_ID,0,"psqlc");
      return &pp_psqlc;
    }
    else if (is_protocol_port(pp_mysqlc.name, port)) {
      LOGBINs(SYS_PP_ID,0,"mysqlc");
      return &pp_mysqlc;
    }
    else if (is_protocol_port(pp_fcgic.name, port)) {
      LOGBINs(SYS_PP_ID,0,"fcgic");
      return &pp_fcgic;
    }
  }

  if (addr->sa_family == AF_FILE) {
    struct sockaddr_un* sun = (struct sockaddr_un *)addr;
    if (strstr(sun->sun_path, "/var/run/postgl")) {
      LOGBINs(SYS_PP_ID,0,"psqlc");
      return &pp_psqlc;
    }
    else if (strstr(sun->sun_path, "/var/run/mysql")) {
      LOGBINs(SYS_PP_ID,0,"mysqlc");
      return &pp_mysqlc;
    }
    else if (strstr(sun->sun_path, "/usr/local/ze")) {
      LOGBINs(SYS_PP_ID,0,"fcgic");
      return &pp_fcgic;
    }
    else if (!strncmp("/tmp/.X11-unix", sun->sun_path, 14)) {
      LOGBINs(SYS_PP_ID,0,"xc");
      return &pp_xc;
    }
  }

  return NULL;
}

pp_dispatcher_t *pp_id_exec(char *foo)
{
  if(bb_disable_pp) return NULL;
  if (strstr(foo, ".fcgi")) {
    LOGBINs(SYS_PP_ID,0,"fcgis");
    return &pp_fcgis;
  }
  return NULL;
}

