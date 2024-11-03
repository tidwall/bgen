#!/usr/bin/env bash

set -e
cd $(dirname "${BASH_SOURCE[0]}")

OK=0
FAILS=
finish() { 
    rm -fr *.o
    rm -fr *.out
    rm -fr *.test
    rm -fr *.profraw
    rm -fr *.dSYM
    rm -fr *.profdata
    rm -fr *.c.worker.js
    rm -fr *.c.wasm
    rm -fr *.out.js
    rm -fr *.out.wasm
    if [[ "$OK" != "1" ]]; then
        echo "FAIL"
    fi
}
trap finish EXIT

rm -rf *.out *.profraw *.profdata *.log

if [[ "$1" == "bench" ]]; then
    ./bench.sh
    OK=1
    exit
fi

if [[ "$SEED" == "" ]]; then
    export SEED=$RANDOM$RANDOM$RANDOM
fi
echo "SEED=$SEED"

if [[ "$RACE" != "1" ]]; then
    echo "For data race check: 'RACE=1 run.sh'"
fi

if [[ "$1" != "" ]]; then
    ./test.sh $1
else
    # test coverage files
    for f in test_*.c; do 
        f="$(echo "$f" | cut -f 1 -d '.')"
        if [[ "$(cat $f.c | grep "#define IGNORE")" == "" ]]; then
        if [[ "$(cat $f.c | grep "#define NOCOV")" == "" ]]; then
            ./test.sh $f 1
        fi
        fi
    done
    # test all the non coverage files
    for f in test_*.c; do 
        f="$(echo "$f" | cut -f 1 -d '.')"
        if [[ "$(cat $f.c | grep "#define IGNORE")" == "" ]]; then
        if [[ "$(cat $f.c | grep "#define NOCOV")" != "" ]]; then
            ./test.sh $f 0
        fi
        fi
    done
fi

OK=1
echo PASSED

# ./cov.sh
