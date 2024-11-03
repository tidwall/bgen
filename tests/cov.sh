#!/usr/bin/env bash

cfile=../bgen.h

set -e
cd $(dirname "${BASH_SOURCE[0]}")

if [[ "$COVREGIONS" == "" ]]; then 
    COVREGIONS="false"
fi
if [[ "$CC" == "" ]]; then
    CC=cc
fi
CCVERSHEAD="$($CC --version | head -n 1)"
if [[ "$CCVERSHEAD" == "" ]]; then
    exit
fi
if [[ "$CCVERSHEAD" == *"clang"* ]]; then
    CLANGVERS="$(echo "$CCVERSHEAD" | awk '{print $4}' | awk -F'[ .]+' '{print $1}')"
    INSTALLDIR="$($CC --version | grep InstalledDir)"
    INSTALLDIR="${INSTALLDIR#* }" 
else
    exit
fi
if [[ -f "$INSTALLDIR/llvm-profdata-$CLANGVERS" ]]; then
    llvm_profdata=$INSTALLDIR/llvm-profdata-$CLANGVERS
elif [[ -f "$INSTALLDIR/llvm-profdata" ]]; then
    llvm_profdata=$INSTALLDIR/llvm-profdata
else
    echo "llvm-profdata missing"
    exit 1
fi
if [[ -f "$INSTALLDIR/llvm-cov-$CLANGVERS" ]]; then
    llvm_cov=$INSTALLDIR/llvm-cov-$CLANGVERS
elif [[ -f "$INSTALLDIR/llvm-cov" ]]; then
    llvm_cov=$INSTALLDIR/llvm-cov
else
    echo "llvm-cov missing"
    exit 1
fi

# echo $llvm_profdata
$llvm_profdata merge $1.profraw -o $1.profdata
$llvm_cov report $1.out $cfile -ignore-filename-regex=.test. \
    -j=4 \
    -show-functions=true \
    -instr-profile=$1.profdata > /tmp/$1.cov.sum.txt

# RED='\033[0;31m'
# NC='\033[0m' # No Color

covered="$(cat /tmp/$1.cov.sum.txt | grep TOTAL | awk '{ print $7; }')"
if [[ "$covered" == "100.00%" ]]; then
    covered="100%"
    printf "%s \e[1;32m%s\e[0m\n" "$1" "$covered"
else
    printf "%s \e[1;31m%s\e[0m\n" "$1" "$covered"
fi
$llvm_cov show $1.out $cfile -ignore-filename-regex=.test. \
    -j=4 \
    -show-regions=true \
    -show-expansions=$COVREGIONS \
    -show-line-counts-or-regions=true \
    -instr-profile=$1.profdata -format=html > /tmp/$1.cov.html

if [[ "$covered" != "100%" ]]; then
    echo "details: file:///tmp/$1.cov.html"
    echo "summary: file:///tmp/$1.cov.sum.txt"
fi

