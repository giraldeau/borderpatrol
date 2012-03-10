#
# File : Makefile
# Desc : Makefile
# Auth : Eric Koskinen
# Date : Wed Oct  4 16:54:48 2006
# RCS  : $Id: Makefile,v 1.35 2009-04-13 18:44:53 ning Exp $
#

OCAMLC=ocamlc
GCC=gcc
CFLAGS=-Wall -Wextra -fPIC
PROF=-pg -g
LIBNAME=libbtrace.so
PREFIX=/usr/local
OPTS=-DTRACE_LIB="\"$(PREFIX)/lib/$(LIBNAME)\""

#ar rc libmylib.a objfile1.o objfile2.o objfile3.o

############################################################################

LIBSRC=vfd.c protocol.c

############################################################################

all: lib libmin pfmon

lib: $(LIBSRC) $(LIBSRC:.c=.h) wrap.h config.h cycles.h syscalls.h binlog.o getservice.o
	$(GCC) $(CFLAGS) $(OPTS) -shared $(LIBSRC) -o $(LIBNAME) -ldl -lpthread ../logd/binlog.o ../logd/fmt.o getservice.o

liba: $(LIBSRC) $(LIBSRC:.c=.h) wrap.h config.h cycles.h syscalls.h binlog.o
	$(GCC) $(CFLAGS) $(OPTS) -c $(PROF) $(LIBSRC)
	ar rc libbtrace.a vfd.o protocol.o ../logd/binlog.o ../logd/fmt.o

libmin: libmin.c
	$(GCC) $(CFLAGS) $(OPTS) -shared libmin.c -o libmin.so -ldl -lpthread

getservice.o: getservice.c getservice.h
	$(GCC) $(CFLAGS) -c -lpthread -D_GNU_SOURCE $^

libmin.c: vfd.c mklibmin.pl
	./mklibmin.pl > libmin.c

binlog.o:
	cd ../logd && make binlog.o
	cd ../logd && make fmt.o

syscalls.h: vfd.c protocol.c
	sh mksyscalls.sh

libpre: vfd.c vfd.h protocol.c protocol.h wrap.h config.h
	$(GCC) $(CFLAGS) $(OPTS) -E vfd.c protocol.c

pfmon:	pf.c config.h ../pfdura/pfdura.h
	$(GCC) $(CFLAGS) $(OPTS) -o pfmon pf.c ../logd/fmt.o -lpthread

install: lib libmin
	install -m 0644 $(LIBNAME) $(PREFIX)/lib/$(LIBNAME)
	install -m 0644 libmin.so  $(PREFIX)/lib/libmin.so

uninstall:
	rm -f $(PREFIX)/lib/$(LIBNAME)
	rm -f $(PREFIX)/lib/libmin.so

min:	min.c min.h
	$(GCC) $(CFLAGS) $(OPTS) -shared min.c -o min.so -ldl
	$(GCC) $(CFLAGS) -o exec-test exec-test.c

clean:
	rm -f *.{so,cmo,cmi} exec-test $(BINNAME) $(LIBNAME) libmin.so libmin.c pfmon *~ *.o

############################################################################

