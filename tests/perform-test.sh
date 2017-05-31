#!/bin/sh

# grab parameters
SQMERGE=${1}
OUTDIR=${2}
DELTA=${3}

# figure all the correct paths out
BASEPATH=${DELTA%.sqdelta}
IN=${BASEPATH}.in
OUT=${BASEPATH}.out

BASENAME=${BASEPATH##*/}
TESTOUT=${OUTDIR}/${BASENAME}.testout

set -e -x

# check if we can apply the delta
"${SQMERGE}" "${IN}" "${DELTA}" "${TESTOUT}"
# and if it matches the reference output
cmp "${TESTOUT}" "${OUT}"
