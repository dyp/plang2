#!/bin/sh

CUR_DIR=$1
BIN_DIR=$2
TEST=$3

shift 3
echo "[ Test   ]" `basename $TEST`

EXPECTED=$TEST.dot
RESULT=`mktemp`

$BIN_DIR/verification_tests -m $TEST |sort > $RESULT

sort $EXPECTED| diff -b -B $RESULT - >/dev/null 2>&1

if [ "$?" = "0" ]; then
    echo -e "[ \033[0;32mPassed\033[0m ]"
    rm $RESULT
    exit 0
else
    echo -e "[ \033[0;31mFailed\033[0m ]"
    diff -u $RESULT $EXPECTED
    rm $RESULT
    exit 1
fi
