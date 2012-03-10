GCC=gcc
CFLAGS=-Wall -W -fPIC
#HEADERS=../trace/wrap.h ../trace/config.h sysrender.h ../trace/binlog.h
PREFIX=/usr/local

#########################################################

all: logd expand

install: logd expand
	install -m 0755 logd $(PREFIX)/bin/logd
	install -m 0755 expand $(PREFIX)/bin/expand

uninstall:
	rm -f $(PREFIX)/bin/logd
	rm -f $(PREFIX)/bin/expand

#########################################################

logd: logd.c fmt.o common.o
	$(GCC) $(CFLAGS) -g -o logd logd.c fmt.o common.o

expand: expand.c common.o binlog.o fmt.o sysrender.h
	$(GCC) $(CFLAGS) -g -o expand fmt.o binlog.o expand.c common.o -lpthread -ldl

sizeof:	sizeof.c fmt.h
	$(GCC) $(CFLAGS) -o sizeof sizeof.c -lpthread

test: test.c fmt.h fmt.c
	$(GCC) $(CFLAGS) -o test test.c fmt.c -lpthread

sysrender.h: mksysrender.sh ../trace/vfd.c ../trace/protocol.c
	cd ../trace && make syscalls.h
	sh mksysrender.sh

#########################################################

fmt.o: fmt.c fmt.h sysrender.h
	$(GCC) $(CFLAGS) -g -c fmt.c

binlog.o: binlog.c binlog.h sysrender.h
	$(GCC) $(CFLAGS) -g -c binlog.c

common.o: common.c common.h sysrender.h
	$(GCC) $(CFLAGS) -g -c common.c

#########################################################

clean:
	rm -f logd expand *.o sysrender.h *~ sizeof test
