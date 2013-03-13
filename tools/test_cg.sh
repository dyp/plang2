#!/bin/sh

CUR_DIR=$1
BIN_DIR=$2
TEST=$3

shift 3
echo "[ Test   ]" `basename $TEST`

EXPECTED=$TEST.dot
RESULT=`mktemp`

$BIN_DIR/plang -pcg $@ $TEST > $RESULT
diff -b $RESULT $EXPECTED >/dev/null 2>&1

if [ "$?" = "0" ]; then
    echo "[ \033[0;32mPassed\033[0m ]"
    rm $RESULT
    exit 0
else
    echo "[ \033[0;31mFailed\033[0m ]"
    diff -u $RESULT $EXPECTED
    rm $RESULT
    exit 1
fi
