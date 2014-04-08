#!/bin/sh

CUR_DIR=$1
BIN_DIR=$2
TEST=$3

shift 3
$BIN_DIR/verification_tests $@ $TEST 2>&1 |$CUR_DIR/test.py $TEST
