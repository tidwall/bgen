#!/usr/bin/env bash

set -e
cd $(dirname "${BASH_SOURCE[0]}")

if [[ "$CC" == "" ]]; then
    CC=cc
fi
CCVERSHEAD="$($CC --version | head -n 1)"
if [[ "$CCVERSHEAD" == "" ]]; then
    exit 1
fi
if [[ "$CCVERSHEAD" == *"Emscripten"* ]]; then
    NOSANS=1
    EMCC=1
fi
if [[ "$VALGRIND" == "1" || "$CC" == *"zig"* ]]; then
    NOSANS=1
fi
if [[ "$CCVERSHEAD" == *"clang"* && "$NOSANS" != "1" ]]; then
    CFLAGS="-O0 -g3 $CFLAGS"
    CFLAGS="-fno-omit-frame-pointer $CFLAGS"
    CFLAGS="-fprofile-instr-generate $CFLAGS"
    CFLAGS="-fcoverage-mapping $CFLAGS"
    CFLAGS="-fsanitize=undefined $CFLAGS"
    CFLAGS="-fno-inline $CFLAGS"
    if [[ "$RACE" == "1" ]]; then
        CFLAGS="$CFLAGS -fsanitize=thread"
    else 
        CFLAGS="-fsanitize=address $CFLAGS"
    fi
fi
CFLAGS="-Wall -Wextra -Wextra $CFLAGS"
CFLAGS="-pedantic -fstrict-aliasing $CFLAGS"
# CFLAGS="-Wsign-compare -Wsign-conversion -Wshadow $CFLAGS"
if [[ "$EMCC" == "1" ]]; then
    CFLAGS="$CFLAGS -pthread -sINITIAL_MEMORY=134610944"
fi

printf "\e[2m%s\e[0m\n" "$CC $CFLAGS $1.c"

echo $CC $CFLAGS $1.c >> test.log
$CC $CFLAGS $1.c

if [[ "$EMCC" == "1" ]]; then
    mv a.out.js $1.out.js
    # mv a.out.wasm $1.out.wasm
else
    mv a.out $1.out
fi
