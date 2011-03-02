#!/bin/env python

import sys
import re
import os.path

def extractTest(_fileName):
    bc = 0      # Block comment
    t = False   # Test
    s = ""
    for ln in open(_fileName):
        lc = False  # Line comment
        tc = False  # In-test comment
        while ln:
            if bc == 0 and ln.startswith("//"):
                lc = True
                ln = ln[2:]
            if ln.startswith("/*"):
                bc += 1
                ln = ln[2:]
            if ln.startswith("*/"):
                bc -= 1
                ln = ln[2:]
            if lc or bc > 0:
                if ln.startswith("<test>"):
                    t = True
                    ln = ln[len("<test>"):]
                if t and ln.startswith("</test>"):
                    t = False
                    ln = ln[len("</test>"):]
                if ln and t and ln[0] == '#':
                    tc = True
                if ln and t and not tc:
                    s += ln[0]
            ln = ln[1:]
        if t:
            s += "\n"
    return s

def makeRegEx(_test):
    s = ""
    if _test[0] != '/':
        s = r"[^=]*(?=/)"
        _test = "/" + _test

    for p in re.split(r"/(?=/)", _test):
        if p:
            s += re.sub(r"/([^|/]*)(?=(/|\Z))", r"/([^/=]*\||)\1(\|[^/=]*|)", p)
        else:
            s += r"/[^=]*"

    return s + r"[^/=]*(=|\Z)"

if len(sys.argv) < 2:
    print "No filename given"
    exit(1)

print "[ Test   ]", os.path.basename(sys.argv[1])

lns = sys.stdin.readlines()

for test in extractTest(sys.argv[1]).split('\n'):
    test = test.strip()
    if not test:
        continue

    msgPassed = "[ \033[0;32mPassed\033[0m ]"
    msgFailed = "[ \033[0;31mFailed\033[0m ]"
    printGots = True

    idx = test.find('=')
    val = None
    cond = test
    if idx >= 0:
        val = test[idx + 1:].strip()
        cond = test[:idx].strip()

    if cond.startswith("!"):
        msgPassed, msgFailed = msgFailed, msgPassed     # Swap.
        cond = cond[1:].strip()
        printGots = False

    cond = makeRegEx(cond)
    match = None
    gots = set()

    for ln in lns:
        ln = ln.strip()
        if not ln:
            continue

        match = re.match(cond, ln)
        if not match:
            continue

        if not val:
            break

        # Compare right-hand sides.
        got = match.string[match.end():].strip()
        if got != val:
            # Compare parts of last member, i.e.: /foo/bar/...|BAZ|...
            if re.match(r".*/([^/=]*\||)" + val + r"(\|[^/=]*|\s*)(=|\Z)", ln):
                break
            gots.add(got)
            match = None
        else:
            break

    if not match:
        if gots and printGots:
            gots = "(got '" + "', '".join(gots) + "')"
        else:
            gots = ""
        print msgFailed, test, gots
        continue

    print msgPassed, test

