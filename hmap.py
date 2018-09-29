#!/usr/bin/env python

import sys
from intervaltree import IntervalTree, Interval

tree = IntervalTree()

f = file(sys.argv[1])
print "digraph G {"
for line in f:
    if line.startswith('WALK'): continue
    if line.startswith('EDGE'): break
    start, size = map(lambda i: int(i,16), line.split())
    tree.add(Interval(start,start+size))
    print "n_%016x" % start

for line in f:
    src, dst = map(lambda i: int(i,16), line.split())
    si = tree[src]
    di = tree[dst]
    if si and di:
        print "n_%016x -> n_%016x" % (si.pop().begin, di.pop().begin) 
print "}"
