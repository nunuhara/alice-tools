#!/usr/bin/env bash

cd $(dirname "$0")

FAILED=0
NTESTS=0

function run_test {
    NTESTS=$((NTESTS+1))
    if ! ./test-runner.sh $@; then
        FAILED=$((FAILED+1))
    fi
}

run_test arith_int.jaf
run_test arith_float.jaf
run_test arith_lint.jaf 5
run_test assign_int.jaf
run_test assign_int.jaf assign_int.v12 12
run_test assign_float.jaf
run_test assign_lint.jaf 5
run_test incdec.jaf
run_test incdec.jaf incdec.v12 12
run_test string.jaf
run_test class.jaf
run_test class.jaf class.v12 12
run_test call.jaf
run_test call.jaf call.v12 12
run_test syscall.jaf
run_test ifacecall.jaf 12
run_test control.jaf
run_test jump.jaf
run_test functype.jaf
run_test delegate.jaf 6

echo Passed: $((NTESTS - FAILED))/$NTESTS
echo Failed: $FAILED/$NTESTS

if (( FAILED > 0 )); then
    exit 1
fi
