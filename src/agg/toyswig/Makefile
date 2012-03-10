
PREFIX=/research/instance/active

EXTERN=$(PREFIX)/src/logd/binlog.o $(PREFIX)/src/logd/common.o

################################################################3

all: swig.cmo bridge.cmo
	ocamlc -custom -o run $(EXTERN) swig.cmo bridge_wrap.o bridge.cmo run.ml

top:
	ocamlmktop -custom -o tmp $(EXTERN) swig.cmo bridge_wrap.o bridge.cmo run.ml

swig.cmo:
	swig -ocaml -co swig.mli ; swig -ocaml -co swig.ml
	ocamlc -c swig.mli ; ocamlc -c swig.ml

bridge.cmo:
	swig -ocaml bridge.i
	ocamlc -c -ccopt "-I/usr/include" bridge_wrap.c
	rm bridge.mli # this is a bogus mli
	ocamlc -c bridge.ml

clean:
	rm -f *.{cmo,mli,o,so,cmi} *_wrap.[co] ttop run *~ bridge.ml swig.ml tmp

################################################################3
