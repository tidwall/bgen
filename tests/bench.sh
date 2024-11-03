#!/usr/bin/env bash

set -e
cd $(dirname "${BASH_SOURCE[0]}")

if [[ "$CC" == "" ]]; then
    CC=cc
fi

if [[ "$CXX" == "" ]]; then
    CXX=c++
fi

if [[ "$G" == "" ]]; then
    export G=5
fi

echo "tidwall/bgen"
$CC -O3 $CFLAGS bench_b.c
./a.out

echo
echo "tidwall/bgen (spatial)"
$CC -O3 $CFLAGS bench_s.c
./a.out
