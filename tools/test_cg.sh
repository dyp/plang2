#!/bin/sh

CUR_DIR=$1
BIN_DIR=$2
TEST=$3

shift 3
echo "[ Test   ]" `basename $TEST`

EXPECTED=$TEST.dot
RESULT=`mktemp`
EXPECTED2=`mktemp`
cp $EXPECTED $EXPECTED2

$BIN_DIR/plang -pcg -k $@ $TEST > $RESULT
sccmap -d -S $TEST.dot >> $EXPECTED2
$CUR_DIR/test_cg_sort.py $RESULT
$CUR_DIR/test_cg_sort.py $EXPECTED2
diff -b $RESULT $EXPECTED2 >/dev/null 2>&1

if [ "$?" = "0" ]; then
    echo "[ \033[0;32mPassed\033[0m ]"
    rm $RESULT
    rm $EXPECTED2
    exit 0
else
    echo "[ \033[0;31mFailed\033[0m ]"
    diff -u -b $RESULT $EXPECTED2
    rm $RESULT
    rm $EXPECTED2
    exit 1
fi
