#!/usr/bin/env bash

set -e
cd $(dirname "${BASH_SOURCE[0]}")

rm -f a.out
rm -f a.out.js
rm -f a.out.wasm
rm -f $1.out
rm -f $1.out.js
rm -f $1.out.wasm
rm -f default.profraw
rm -f $1.profraw

./build.sh "$1"

export MallocNanoZone=0
if [[ "$VALGRIND" == "1" ]]; then
    valgrind --leak-check=yes ./$1.out
elif [[ -f "$1.out.js" ]]; then
    node ./$1.out.js
else
    ./$1.out
fi

if [[ $(cat $1.c | grep "#define NOCOV") == "" ]]; then
    if [[ -f default.profraw ]]; then
        mv default.profraw $1.profraw
        ./cov.sh $1
    fi
# else 
    # echo covered: ignored
fi
