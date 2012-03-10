#
# * File : Makefile
# * Desc : Makefile for everything
# * Auth : Eric Koskinen
# * Date : Fri Nov 10 15:09:59 2006
# * RCS  : $Id: Makefile,v 1.10 2009-04-21 03:10:47 ning Exp $
#

TOOLS0=/research/tools/v1.0
TOOLS1=/research/tools/v1.1
ROOT=/research/instance/active
BASEDIR=`basename $(CURDIR)`
FILELIST=$(BASEDIR)/bin/analyze \
	$(BASEDIR)/bin/print_requests.py \
	$(BASEDIR)/bin/print_paths.py \
	$(BASEDIR)/etc/debug-schema.sql \
	$(BASEDIR)/Makefile \
	$(BASEDIR)/README \
	$(BASEDIR)/src/vis/build-ranges.sql \
	$(BASEDIR)/src/agg \
	$(BASEDIR)/src/logd \
	$(BASEDIR)/src/trace \
	$(BASEDIR)/src/pfdura \
	$(BASEDIR)/src/Makefile

##################################################################

all: blib logd agg

blib:
	cd src/trace && make

logd:
	cd src/logd && make

agg:
	cd src/agg && make

vis:
	cd src/vis && make

bench:
	cd src/bench && make

install: blib logd agg
	cd src/trace && make install
	cd src/logd && make install
	cd src/agg && make install
#	cd src/vis && make install

clean:
	cd src/trace && make clean
	cd src/logd && make clean
	cd src/agg && make clean
#	cd src/vis && make clean

##################################################################

ebench: blib
	cd src/bench && make

##################################################################

tclean: blib logd
	-killall -q logd.bin
	-killall -q httpd
	rm -f /tmp/logd_log

tlog: tclean install
	bin/logd /tmp/fifo /tmp &

tapache2: tlog
	LD_PRELOAD=src/trace/libbtrace.so $(TOOLS1)/bin/httpd \
		-f $(ROOT)/etc/httpd.conf-2.2.3

tapache1: tlog
	LD_PRELOAD=src/trace/libbtrace.so $(TOOLS0)/bin/httpd \
		-f $(ROOT)/etc/httpd.conf

tbench: tlog bench

##################################################################

package:
	cd ../ ; tar czvf $(BASEDIR).tar.gz $(FILELIST)

##################################################################