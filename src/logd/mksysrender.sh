#!/bin/sh

echo "#ifndef SYSRENDER_H" > sysrender.h
echo "#define SYSRENDER_H" >> sysrender.h
echo "#include \"../trace/syscalls.h\"" >> sysrender.h
echo "#include <stdlib.h>" >> sysrender.h
echo "char *syscall2string(int c) {" >> sysrender.h
echo "  char *buf;" >> sysrender.h
echo "  switch (c) {" >> sysrender.h
grep SYS_ ../trace/syscalls.h | perl -p -e 's/^#define (.*) \d+$/case $1: return "$1";/' \
    >> sysrender.h
echo "  default: " >> sysrender.h
echo "    buf = malloc(24);" >> sysrender.h
echo "    sprintf(buf,\"unknown:%d\",c);" >> sysrender.h
echo "    return buf;" >> sysrender.h
echo "  }" >> sysrender.h
echo "}" >> sysrender.h
echo "#endif" >> sysrender.h
