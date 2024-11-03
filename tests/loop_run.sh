#!/usr/bin/env bash

set -e
cd $(dirname "${BASH_SOURCE[0]}")

./build.sh test_btree

export MallocNanoZone=0

while :
do
    ./test_btree.out
done