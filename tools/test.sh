#!/bin/sh

CUR_DIR=$1
BIN_DIR=$2
TEST=$3
$BIN_DIR/plang -pflat -t1 $TEST |$CUR_DIR/test.py $TEST
