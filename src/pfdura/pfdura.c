/* pfdura.c
 *
 * user space code to read data from the pfdura-mod module's
 * relayfs
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include "pfdura.h"


char *dirname = "/mnt/relay/pfdura";
char *filname = "cpu0";


static
void *
reader_thr(void *data)
{
	int rc, fd;
	struct pfdura_t pfd;

	fd = (int)data;

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

		printf("TGID: %li\tPID: %li\t%s\t[%li.%li]\n",
			pfd.tgid, pfd.pid,
			pfd.complete ? "end" : "begin",
			pfd.tv.tv_sec, pfd.tv.tv_usec);
	} while (1);
}


static
void
control_write(const char *dirname, const char *filename, long val)
{
	int rc, fd;
	char tmp[4096];

	sprintf(tmp, "%s/%s", dirname, filename);
	fd = open(tmp, O_RDWR);
	if (fd < 0) {
		printf("pfdura: open() error on %s: %s\n",
			tmp, strerror(errno));
		exit(1);
	}

	sprintf(tmp, "%li", val);
	rc = write(fd, tmp, strlen(tmp));
	close(fd);
	if (rc < 0) {
		printf("pfdura: write() error on %s: %s\n",
			tmp, strerror(errno));
		exit(1);
	}
}


int main(int argc, char **argv)
{
	int signal, rfd;
	long pid;
	sigset_t signals;
	char tmp[4096], *cp;
	pthread_t rthr;

	if (argc == 2) {
		pid = strtol(argv[1], &cp, 10);
		if (cp != argv[1])
			goto okay;
	}

	printf("pfdura: no PID specified\n");
	exit(0);

okay:
	sigemptyset(&signals);
	sigaddset(&signals, SIGINT);
	sigaddset(&signals, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &signals, NULL);

	sprintf(tmp, "%s/%s", dirname, filname);
	rfd = open(tmp, O_RDONLY);
	if (rfd < 0) {
		printf("pfdura: open() error on %s: %s\n",
			tmp, strerror(errno));
		return -1;
	}

	if (pthread_create(&rthr, NULL, reader_thr, (void *)rfd) < 0) {
		close(rfd);
		printf("pfdura: pthread_create() error\n");
		return -1;
	}

	control_write(dirname, "on", pid);

	sigemptyset(&signals);
	sigaddset(&signals, SIGINT);
	sigaddset(&signals, SIGTERM);

	while (sigwait(&signals, &signal) == 0) {
		switch(signal) {
		case SIGINT:
		case SIGTERM:
			control_write(dirname, "off", pid);
			pthread_cancel(rthr);
			close(rfd);
			exit(0);
		}
	}
}

