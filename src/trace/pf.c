//
// $Id: pf.c,v 1.12 2009-03-29 03:33:22 ning Exp $
//
// user space code to read data from the pfdura-mod module's relayfs
//

// #################################################################

#define _GNU_SOURCE             /* strnlen() needs this */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include "../pfdura/pfdura.h"
#include "../logd/fmt.h"

//#include <unistd.h>

#include "config.h"

int logfd = -1;
struct sockaddr_in logd_addr;

struct timeval last_tvs[65536];

// #################################################################

static void * reader_thr(void *data) {
  int rc, fd, i;
  struct pfdura_t pfd;

  fd = (int)data;

  /* *************************************** */
  /* Connect to logd */

#ifdef LOGD_USE_SOCK
  char *host_ip = getenv("LIBBTRACE_HOST");
  int host_port = LOGD_PORT;
  if (!host_ip) {
    host_ip = LOGD_IP;
  }
  if (getenv("LIBBTRACE_PORT")) {
    host_port = atoi(getenv("LIBBTRACE_PORT"));
  }

  fprintf(stderr,"--- pf feed init\n"); fflush(stderr);
  if((logfd = socket(PF_INET, SOCK_STREAM, 0))<0) {
    fprintf(stderr,"couldn't create socket\n");fflush(stderr);
    exit(-1);
  }
  /* Construct the server address structure */
  bzero(&logd_addr, sizeof(logd_addr));
  logd_addr.sin_family      = AF_INET;
  logd_addr.sin_addr.s_addr = inet_addr(host_ip);
  logd_addr.sin_port        = htons(host_port);
  /* Establish the connection to the echo server */
  if (connect(logfd, (struct sockaddr *) &logd_addr,
              sizeof(logd_addr)) < 0) {
    fprintf(stderr,"couldn't connect\n");
    fflush(stderr);
    exit(-1);
  }
#else
  if( (logfd=open(LOGD_PIPE, O_WRONLY)) < 0 ) {
    fprintf(stderr,"couldn't connect to named pipe: %s\n", LOGD_PIPE);
    fflush(stderr); exit(1);
  }
#endif
  fprintf(stderr,"--- connected to logd on fd=%d.\n",logfd);
  fflush(stderr);

  for (i=0;i<65536;i++) {
    last_tvs[i].tv_sec = 0;
    last_tvs[i].tv_usec = 0;
  }
  fprintf(stderr,"--- flushed last_tvs\n");

  /* *************************************** */

  do {
    rc = read(fd, &pfd, sizeof(pfd));
    if (rc != sizeof(pfd)) {
      if (rc == 0  ||  errno == EINTR) {
        usleep(100);
        continue;
      }
      printf("pfdura: read() error: %s\n", strerror(errno));
      exit(1);
    }
    // TODO : this table should be indexed on pid too
    if(!pfd.complete) {
      last_tvs[pfd.tgid] = pfd.tv;
    } else {
      long diff_sec = pfd.tv.tv_sec - last_tvs[pfd.tgid].tv_sec;
      long us = (diff_sec > 0 ? (diff_sec * 1000 * 1000) : 0);
      us += pfd.tv.tv_usec - last_tvs[pfd.tgid].tv_usec;

      if(us > 100) {
        logentry_thaw_t le;
        memset(&le, 0, sizeof(le));

        /* TODO: Is pid or tgid the process ID? */
        le.fz.tid = pfd.pid;
        le.fz.pid = pfd.tgid;
        le.fz.cycle_high = last_tvs[pfd.tgid].tv_sec;
        le.fz.cycle_low = last_tvs[pfd.tgid].tv_usec;
        le.fz.sc = SYS_PAGEFAULT_BEGIN;
        le.fz.nm = '_';

        int sz = frozen_size(&(le.fz));
        char *buf = malloc(sz);
        freeze(buf, &le, sz);
        write(logfd, buf, sz);

        le.fz.cycle_high = pfd.tv.tv_sec;
        le.fz.cycle_low = pfd.tv.tv_usec;
        le.fz.sc = SYS_PAGEFAULT_END;

        freeze(buf, &le, sz);
        write(logfd, buf, sz);

        free(buf);
      }
    }
    
  } while (1);

}

// #################################################################


int main(void)
{
	int signal, rfd;
	sigset_t signals;
	pthread_t rthr;

	sigemptyset(&signals);
	sigaddset(&signals, SIGINT);
	sigaddset(&signals, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &signals, NULL);

	rfd = open(RELAY_FEED, O_RDONLY);
	if (rfd < 0) {
		printf("pf: open() error on %s: %s\n",
           RELAY_FEED, strerror(errno));
		return -1;
	}

	if (pthread_create(&rthr, NULL, reader_thr, (void *)rfd) < 0) {
		close(rfd);
		printf("pf: pthread_create() error\n");
		return -1;
	}

	sigemptyset(&signals);
	sigaddset(&signals, SIGINT);
	sigaddset(&signals, SIGTERM);

	while (sigwait(&signals, &signal) == 0) {
	  switch(signal) {
	  case SIGINT:
	  case SIGTERM:
	    //control_write(dirname, "off", tgid);
	    if(logfd>0) close(logfd);
	    pthread_cancel(rthr);
	    close(rfd);
	    return 0;
	  }
	}

  return 0;
}

