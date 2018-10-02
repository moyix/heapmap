#!/usr/bin/env python

import sys
from intervaltree import IntervalTree, Interval
import argparse

parser = argparse.ArgumentParser(description="create DOT of heap graph")
parser.add_argument("--edges-only", action='store_true')
parser.add_argument('files', type=str, nargs='+')
args = parser.parse_args()

tree = IntervalTree()

for fname in args.files:
    f = file(fname)
    of = open(fname + '.dot','w')
    print >>of, "digraph G {"
    for line in f:
        if line.startswith('WALK'): continue
        if line.startswith('EDGE'): break
        start, size = map(lambda i: int(i,16), line.split())
        tree.add(Interval(start,start+size))
        if not args.edges_only:
            print >>of, "n_%016x" % start

    for line in f:
        src, dst = map(lambda i: int(i,16), line.split())
        si = tree[src]
        di = tree[dst]
        if si and di:
            print >>of, "n_%016x -> n_%016x" % (si.pop().begin, di.pop().begin) 
    print >>of, "}"
