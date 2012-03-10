#!/bin/sh

# extract SYS_* from vfd.c and give them numbers,
#   thus generating syscalls.h

echo "#ifndef SYSCALLS_H" > syscalls.h
echo "#define SYSCALLS_H" >> syscalls.h
grep SYS_ vfd.c protocol.c pf.c | grep -v 'SYS_,' | \
   perl -p -e 's/^.*SYS_([\w]+).*$/SYS_$1/' | grep -v '//' | sort | uniq | \
   perl -e '@l = <STDIN>; my $i=0; for(@l) { chomp; print "\#define $_ $i\n"; $i++ }'\
    >> syscalls.h
echo "#endif" >> syscalls.h
