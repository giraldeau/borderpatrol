#
# $Id: Makefile,v 1.15 2009-04-03 00:53:00 ning Exp $
#

SOURCE=events.ml parse.ml db.ml join.ml
FLAGS=-g
GCC=gcc
PSQL=-I +postgresql postgresql.cma
PREFIX=../..
EXTERN=$(PREFIX)/src/logd/binlog.o $(PREFIX)/src/logd/common.o $(PREFIX)/src/logd/fmt.o $(PREFIX)/src/trace/getservice.o $(PSQL)
# use swig 1.3.30 because the new version change the variable
# name conventions. ugh!
SWIG=swig
#$(PREFIX)/bin/swig

################################################################3

all: agg

agg: syscalls.ml $(SOURCE) agg.ml swig.cmo bridge.cmo
	ocamlc -custom $(FLAGS) -o agg unix.cma bigarray.cma -I +threads threads.cma $(EXTERN) \
	  str.cma swig.cmo bridge_wrap.o bridge.cmo syscalls.ml $(SOURCE) agg.ml
	ocamlmktop -custom $(FLAGS) -o top unix.cma bigarray.cma -I +threads threads.cma $(EXTERN) \
	  str.cma swig.cmo bridge_wrap.o bridge.cmo syscalls.ml $(SOURCE)

test: $(SOURCE) test.ml
	ocamlc $(FLAGS) str.cma $(SOURCE) test.ml -o test

swig.cmo:
	$(SWIG) -ocaml -co swig.mli ; $(SWIG) -ocaml -co swig.ml
	ocamlc -c swig.mli ; ocamlc -c swig.ml

bridge.cmo: bridge.i ../logd/fmt.h
	$(SWIG) -ocaml bridge.i
	ocamlc -c -ccopt -w -ccopt "-I/usr/include" bridge_wrap.c
	rm bridge.mli # this is a bogus mli
	ocamlc -c bridge.ml

syscalls.ml: ../trace/vfd.c ../trace/protocol.c ../trace/config.h Makefile
	cd ../trace && make syscalls.h
	rm -f syscalls.ml
	echo "exception Syscall_render_error"        > syscalls.ml
	echo "exception Protocol_render_error"      >> syscalls.ml
	echo "let sc_str_of_int i = "               >> syscalls.ml
	echo "  match i with"                       >> syscalls.ml
	cat ../trace/syscalls.h | perl -n \
	  -e 'if (/ (SYS_[^ ]*) (\d+)$$/) { print "   | $$2 -> \"$$1\"\n" }' >> syscalls.ml
	echo "   | _ -> \"Syscall_render_error\"" >> syscalls.ml
	echo ";;"                                   >> syscalls.ml
	echo "let str_of_ppid i = "                 >> syscalls.ml
	echo "  match i with"                       >> syscalls.ml
	cat ../trace/config.h | perl -n \
	  -e 'if (/ (PP_[^ ]*_ID) (\d+)$$/) { print "   | $$2 -> \"$$1\"\n" }' >> syscalls.ml
	echo "   | _ -> Printf.printf \"sc=%d\n\" i; raise Protocol_render_error">> syscalls.ml
	echo ";;"                                   >> syscalls.ml

install:
	install -D -m 0755 agg $(PREFIX)/bin/agg

clean:
	rm -f *.cmo *.mli *.o *.so *.cmi agg top *~ swig.ml *_wrap.[co] bridge.ml syscalls.ml

################################################################3
