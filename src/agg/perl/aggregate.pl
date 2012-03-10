#!/usr/bin/perl -w
#
# $Id: aggregate.pl,v 1.1 2006-11-30 18:59:33 ejk Exp $
#
# Script to parse the log file and extract the tree of LaborNodes
#

use lib '/research/instance/active/src/lib';

use Parser qw/parse_traces/;
use Interfaces qw/annotate_interfaces/;
use LaborTree qw/find_labor_nodes assemble_labor_nodes/;
use Protocol qw/pp_annotate_starts/;

use Data::Dumper;
use strict;

######################################################################

my $traces = parse_traces('/research/instance/active/logs/logd_log');

$traces = pp_annotate_starts($traces);

$traces = annotate_interfaces($traces,1);

# print "ANNOTATED -->>>\n".Dumper($traces);

my $allLaborNodes = find_labor_nodes($traces,0);

# print "LABOR NODES -->>>\n".Dumper($allLaborNodes);

my $roots = assemble_labor_nodes($allLaborNodes);

# find all the starting points of requests (make @reqs)

# follow requests, using reg->do_fork, etc

##############################################################################

exit();
