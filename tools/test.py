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
failed = 0

for test in extractTest(sys.argv[1]).split('\n'):
    test = test.strip()
    if not test:
        continue

    msgPassed = "[ \033[0;32mPassed\033[0m ]"
    msgFailed = "[ \033[0;31mFailed\033[0m ]"
    printGots = True
    neededResult = True

    idx = -1
    op = ""
    match = re.search("[=~]", test)
    #idx = test.find('=')
    val = None
    cond = test

    if match:
        op = test[match.start()]
        idx = match.start()
        val = test[idx + 1:].strip()
        cond = test[:idx].strip()

    if cond.startswith("!"):
        cond = cond[1:].strip()
        printGots = False
        neededResult = False

    cond = makeRegEx(cond)
    match = None
    gots = set()

    if val:
        val = re.escape(val)
        if op == "~":
            val = r".*\b" + val + r"\b.*"

    for ln in lns:
        ln = ln.strip()
        if not ln:
            continue

        match = re.match(cond, ln)
        if not match:
            continue

        # Empty string is a valid value.
        if val == None:
            break

        # Compare right-hand sides.
        got = match.string[match.end():].strip()
        if not re.match(r"\A" + val + r"\Z", got):
            # Compare parts of last member, i.e.: /foo/bar/...|BAZ|...
            if re.match(r".*/([^/=]*\||)" + val + r"(\|[^/=]*|\s*)(=|\Z)", ln):
                break
            gots.add(got)
            match = None
        else:
            break

    success = (not match) == (not neededResult)
    msg = msgPassed if success else msgFailed

    if not success:
        failed += 1

    if not match:
        gots = "(got '" + "', '".join(gots) + "')" if gots and printGots else ""
        print msg, test, gots
        continue

    print msg, test

sys.exit(0 if not failed else -1)
