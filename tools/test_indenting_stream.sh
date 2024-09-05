#!/bin/sh

EXECUTABLE=$1
TEST=$2

shift 2
echo "[ Test   ]" `basename $TEST`

EXPECTED=$TEST.out
RESULT=`mktemp`

$EXECUTABLE $TEST.in > $RESULT
diff $RESULT $EXPECTED >/dev/null 2>&1

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
