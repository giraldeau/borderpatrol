//
// $Id: logd.c,v 1.28 2009-04-13 16:36:10 ning Exp $
//

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "fmt.h"
#include "common.h"

// ##########################################################

#define DEBUG_LOG 0

#define RELAY_BUF 4*1024*1024 // PIPE_BUF

// ##########################################################

char *relay[FD_SETSIZE];
int infd = -1, outfd = -1;
fd_set active_fd_set;

unsigned int b_end[FD_SETSIZE];
unsigned int avail[FD_SETSIZE];
struct timeval timediff[FD_SETSIZE];

/**
 * Calculate the time difference between the local machine and the remote
 * machine.
 */
void record_timediff(const logentry_frozen_t const *lentry) {
  struct timeval localtime, remotetime;

  if (!lentry || infd == -1) return;

  gettimeofday(&localtime, NULL);
  remotetime.tv_sec = lentry->cycle_high;
  remotetime.tv_usec = lentry->cycle_low;

  timersub(&localtime, &remotetime, &timediff[infd]);
}

/**
 * Apply the time difference to the entry and write it to the log file.
 */
void transfer_out() {
  logentry_frozen_t *frozen = NULL;
  logentry_thaw_t *logentry = NULL;
  struct timeval remotetime, newtime;
  unsigned int size;
  char *buf = NULL;

  if (!relay[infd]) {
    fprintf(stderr, "Invalid relay buffer for file descriptor %d.\n", infd);
    return;
  }

  if (outfd < 0) {
    fprintf(stderr, "invalid output file descriptor.\n");
    return;
  }

  while (b_end[infd] >= sizeof(logentry_frozen_t)) {
    /* Get the length of the string. */
    frozen = thaw1(relay[infd]);

    if (!timerisset(&timediff[infd]))
      record_timediff(frozen);

    size = frozen_size(frozen);
    if (b_end[infd] < size) {
      return;
    }
    logentry = thaw2(relay[infd] + sizeof(logentry_frozen_t), frozen,
                     size - sizeof(logentry_frozen_t));

    /* Apply the time difference to the logentry. */
    remotetime.tv_sec = frozen->cycle_high;
    remotetime.tv_usec = frozen->cycle_low;
    timeradd(&remotetime, &timediff[infd], &newtime);
    logentry->fz.cycle_high = (unsigned long)newtime.tv_sec;
    logentry->fz.cycle_low = (unsigned long)newtime.tv_usec;

    buf = malloc(size);
    freeze(buf, logentry, size);
    int t = write(outfd, buf, size);
    if (DEBUG_LOG) fprintf(stderr,"logd: wrote %d\n",t);

    if(t == -1) { perror("a"); exit(1); }
    if(t > 0) {
      unsigned int i;
      for (i = 0; i < b_end[infd] - size; i++) {
        relay[infd][i] = relay[infd][t + i];
      }

      b_end[infd] -= t;
      avail[infd] += t;
    }

    free(buf);
    free_le(logentry);
    free(frozen);
    buf = NULL;
    logentry = NULL;
    frozen = NULL;
  }
}

/**
 * Reads the data from the file descriptor.
 *
 * Returns the number of bytes read, or -1 on failure. 0 byte on a socket means
 * that the client disconnected.
 */
int transfer_in() {
  if (infd < 0) {
    fprintf(stderr, "invalid input file descriptor.\n");
    return -1;
  }

  if (!relay[infd]) {
    fprintf(stderr, "Invalid relay buffer for file descriptor %d.\n", infd);
    return -1;
  }

  int in = read(infd,&relay[infd][b_end[infd]],avail[infd]);
  if (in == 0) {
    return 0;
  }
  if (in == -1) {
    perror("transfer_in");
    return -1;
  }
  if (DEBUG_LOG) fprintf(stderr,"logd: read %d\n",in);

  avail[infd] -= in;
  b_end[infd] += in;
  if(b_end[infd] > sizeof(logentry_frozen_t)) {
    transfer_out();
  }

  return in;
}

// ##########################################################

void sig() {
  int i;

  if (DEBUG_LOG) fprintf(stderr,"SIGINT. ");
  signal(SIGINT, sig);
  if(outfd < 0) {
    if (DEBUG_LOG) fprintf(stderr,"during init. closing.\n");
  } else {
    if (infd >= 0) {
      close(infd);
    }

    for (i = 0; i < FD_SETSIZE; ++i) {
      if (FD_ISSET(i, &active_fd_set)) {
        close(i);
        FD_CLR(i, &active_fd_set);
      }
    }
    close(outfd);
  }
  exit(0);
}

/**
 * Create a TCP socket on the given port.
 *
 * Returns the socket or -1 on failure.
 */
int create_socket(unsigned short int port) {
  int sock;
  struct sockaddr_in name;

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket");
    return -1;
  }

  name.sin_family = AF_INET;
  name.sin_port = htons(port);
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
    perror("bind");
    return -1;
  }

  return sock;
}

// ##########################################################

int main(int argc, char *argv[]) {
  int r;
  int sock;
  int status;
  fd_set read_fd_set;
  int nfds = 0;
  int avail_fd;
  int i;
  struct sockaddr_in clientname;
  size_t size;
  char *fifo = "/tmp/fifo";
  char *logfn = NULL;
  signal(SIGINT, sig);

  if(argc != 2 && argc != 3) {
    fprintf(stderr,"usage:\tlogd <save.raw>\n");
    fprintf(stderr,"\tlogd <port> <save.raw>\n");
    exit(1);
  }

  /* Initialization */
  for (i = 0; i < FD_SETSIZE; i++) {
    b_end[i] = 0;
    avail[i] = RELAY_BUF;
  }

  if (argc == 2) {              /* Unix pipe */
    logfn = argv[1];

    if( (r=mkfifo(fifo, 0666)) < 0 ) {
      if (errno != EEXIST) {
        perror("logd: mkfifo");
        exit(1);
      }
    }

    if( (infd=open(fifo, O_RDONLY)) < 0 ) {
      fprintf(stderr,"couldn't connect to named pipe: %s\n", fifo);
      perror("logd");
      exit(1);
    }

    relay[infd] = calloc(1, RELAY_BUF);
    FD_ZERO(&active_fd_set);
    FD_SET(infd, &active_fd_set);
    if (infd >= nfds)
      nfds = infd + 1;
  } else {                      /* socket */
    sock = create_socket(atoi(argv[1]));
    logfn = argv[2];

    if (listen(sock, 5) < 0)
    {
      perror("listen");
      exit(EXIT_FAILURE);
    }

    /* Initialize the set of active sockets. */
    FD_ZERO(&active_fd_set);
    FD_SET(sock, &active_fd_set);
    if (sock >= nfds)
      nfds = sock + 1;
  }

  if( (outfd=open(logfn,O_WRONLY | O_CREAT | O_TRUNC,0644)) < 0 ){
    fprintf(stderr,"unable to open %s\n", logfn);
    perror("logd"); exit(1);
  }

  while (1) {
    if (argc >= 2) {            /* socket */
      /* Block until input arrives on one or more active sockets. */
      read_fd_set = active_fd_set;
      if ((avail_fd = select(nfds, &read_fd_set, NULL, NULL, NULL)) < 0) {
        perror("select");
        exit(EXIT_FAILURE);
      }

      /* Service all the sockets with input pending.  */
      for (i = 0; i < nfds; ++i) {
        if (FD_ISSET(i, &read_fd_set)) {
          if (i == sock) {
            /* Connection request on original socket. */
            size = sizeof(clientname);
            if ((status = accept(sock, (struct sockaddr *)&clientname,
                                 &size)) < 0) {
              perror("accept");
              exit(EXIT_FAILURE);
            }
            relay[status] = calloc(1, RELAY_BUF);

            fprintf(stderr, "Server: connect from host %s, port %hu.\n",
                    inet_ntoa(clientname.sin_addr),
                    ntohs(clientname.sin_port));
            FD_SET(status, &active_fd_set);
            if (status >= nfds)
              nfds = status + 1;
          } else {
            /* Data arriving on an already-connected socket. */
            infd = i;
            if (transfer_in() <= 0) {
              fprintf(stderr, "client disconnected or error occurred.\n");
              close(i);
              FD_CLR(i, &active_fd_set);
              if (i == nfds)
                nfds--;

              transfer_out();
              free(relay[i]);
              relay[i] = NULL;
            }
            infd = -1;
          }

          if (--avail_fd == 0) {
            break;
          }
        }
      }
    }
  }
}

// ##########################################################
