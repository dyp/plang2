#!/usr/bin/env python

import sys

if len(sys.argv) < 2:
    print "No filename given"
    exit(1)

start = 0
end = 0

i = 0
j = 0

f = open(sys.argv[1])
tests = f.readlines()

for test in tests:
    i += 1
    j += 1

    if test.rstrip().endswith('{'):
        start = i

    if test.startswith('}'):
        end = j
        tests[start:end] = sorted(tests[start:end])

f = open(sys.argv[1], "w")
f.writelines(tests)
