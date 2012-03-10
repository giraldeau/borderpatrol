#!/usr/bin/perl -w

use strict;

#######################################################

print <<EOT;
#include <dlfcn.h>
#include "wrap.h"
int first_init = 0;

void wrap_init();

void vfd_init(char *caller)
{
  wrap_init();
}

#define NULL_WRAP(r,f,args,anames)		\\
r f args {					\\
  if (!first_init)                              \\
    vfd_init( #f );				\\
  return real_##f anames;			\\
}
EOT

#######################################################

open IN, "vfd.c" or die $!;
my @all = <IN>;
close IN;

my $a = join '', @all;
$a =~ s/\n/___EJK___/g;
$a =~ s/^.*(void wrap_init[^}]*}).*$/$1/mg;
$a =~ s/___EJK___/\n/g;

print $a;

#######################################################

for (@all) {
  next if /^\/\//;
  next unless m/START_WRAP\(([^,]+), ?([^,]+), ?\(([^\)]*)\)/;
  my ($rv,$sc,$args) = ($1,$2,$3);
  next if m/pthread/;
  if ($args =~ /\.\.\./) {
    print "// $rv = $sc ($args)\n";
    next;
  }
  my @anames = map { s/^.* \*?([^, \[\]]+)(\[\d*\])?$//; $1 } split ',', $args;
  my $an = join(',',@anames);

  print <<EOT;

/* SYSCALL: $sc
 * RV TYPE: $rv
 * ARGS   : $args
 * CALL   : $an
 */
EOT
  print "NULL_WRAP($rv, $sc, ($args),($an))\n";
}

#######################################################

exit;
