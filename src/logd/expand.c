//
// $Id: expand.c,v 1.11 2009-03-11 20:14:50 ning Exp $
//

#include "../trace/config.h"
#include "binlog.h"
#include "common.h"
#include "fmt.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

// ##########################################################

extern int bytes_seen;

extern char bb_disable_log; // pointless

// TODO : sigint to close the file

int main(int argc, char *argv[]) {
  int rawfd; int recno = 0;
  unsigned int pid = 0;
  struct stat fstat;

  if(argc < 2) {
    fprintf(stderr,"usage: expand <logfn> [pid]\n");
    exit(1);
  }

  if( (rawfd=open(argv[1], O_RDONLY)) < 0 ) {
    fprintf(stderr,"couldn't connect to raw log: %s\n", argv[1]);
    fflush(stderr); exit(1);
  }

  if(argc == 3) {
    pid = atoi(argv[2]);
  }

  if( stat(argv[1], &fstat) < 0 ) {
    perror("stat");
    exit(1);
  }

  while (1) {
    logentry_thaw_t *le = get_le(rawfd,0); // 0 = non-persistant
    if(le == NULL) break;
    //check_le(le);
    recno++;
    if(BUF_DEBUG) { fprintf(stdout, "r%6d: \n",recno); fflush(stderr); }
    if(pid && le->fz.pid != pid) {
      // no output
    } else {
      output_le(stdout,le);
    }
    free_le(le);
  }

  fprintf(stdout, "total: %d\n",recno);
  fprintf(stdout, "bytes: %d\n",bytes_seen);

  if(fstat.st_size != bytes_seen) {
    fprintf(stdout, "ERROR! size mismatch (fstat=%ld,used=%ld)\n",
	    (long)fstat.st_size,(long)bytes_seen);
    exit(1);
  }

  return 1;
}

// ##########################################################
