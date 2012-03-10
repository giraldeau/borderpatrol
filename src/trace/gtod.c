#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>


int main() {
  struct timeval tv;
  long sn;
  if(gettimeofday(&tv,NULL)) {
    fprintf(stderr,"couldn't get time of day\n");
    fflush(stderr);
    exit(1);
  }
  printf("S: %015ld\n",tv.tv_sec);
  printf("U: %015ld\n",tv.tv_usec);
  sn = (tv.tv_sec - 1144765785) * 1000 + tv.tv_usec;
  printf("TS: %ld\n",sn);
  return 0;
}
