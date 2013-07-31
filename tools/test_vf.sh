#!/bin/sh

CUR_DIR=$1
BIN_DIR=$2
TEST=$3

shift 3
$BIN_DIR/plang -e2 -k $@ $TEST |$CUR_DIR/test.py $TEST
