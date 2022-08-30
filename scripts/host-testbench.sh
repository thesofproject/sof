#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

#
# Usage:
# please run following scrits for the test tplg and host build
# ./scripts/build-tools.sh -t
# ./scripts/rebuild-testbench.sh
# Run test
# ./scripts/host-testbench.sh
#

# stop on most errors
set -e

function filesize() {
  du -b "$1" | awk '{print $1}'
}

function comparesize() {
  INPUT_SIZE=$1
  OUTPUT_SIZE=$2
  INPUT_SIZE_MIN=$(echo "$INPUT_SIZE*0.9/1"|bc)
  # echo "MIN SIZE with 90% tolerance is $INPUT_SIZE_MIN"
  # echo "ACTUALL SIZE is $OUTPUT_SIZE"
  if [[ "$OUTPUT_SIZE" -gt "$INPUT_SIZE" ]]; then
    echo "OUTPUT_SIZE $OUTPUT_SIZE too big"
    return 1
  fi
  if [[ "$OUTPUT_SIZE" -lt "$INPUT_SIZE_MIN" ]]; then
    echo "OUTPUT_SIZE $OUTPUT_SIZE too small"
    return 1
  fi

  return 0
}

function srcsize() {
  INPUT_SIZE=$1
  INPUT_RATE=$2
  OUTPUT_RATE=$3
  OUTPUT_SIZE=$(echo "${INPUT_SIZE}*${OUTPUT_RATE}/${INPUT_RATE}"|bc)
  echo "$OUTPUT_SIZE"
}

function die() {
  printf "$@"
  exit 1
}

process_test_cmd() {
  octave -q --eval "pkg load signal io; [n_fail]=process_test('$1', $2, $3, $4, $5);exit(n_fail)"
}

# function test_component()
# $1 : component name
# $2 : input bits per sample
# $3 : output bits per sample
# $4 : sampling rate
# $5 : quick test=0, fulltest=1
test_component() {
  test "$#" -eq 5 || die "Have $# parameters but 5 expected"

  echo "------------------------------------------------------------"
  echo "test $1 component with $2 $3 $4 $5"
  if process_test_cmd "$@"; then
    echo "$1 test passed!"
  else
    die "$1 test failed!"
  fi
}

SCRIPTS_DIR=$(dirname "${BASH_SOURCE[0]}")
SOF_DIR=$SCRIPTS_DIR/../
TESTBENCH_DIR=${SOF_DIR}/tools/test/audio
INPUT_FILE_SIZE=10240

cd "$TESTBENCH_DIR"
rm -rf ./*.raw

# create input zeros raw file
head -c ${INPUT_FILE_SIZE} < /dev/zero > zeros_in.raw

# by default quick test
FullTest=${FullTest:-0}

# test with volume
test_component volume 16 16 48000 "$FullTest"
test_component volume 24 24 48000 "$FullTest"
test_component volume 32 32 48000 "$FullTest"

# test with eq-iir
test_component eq-iir 16 16 48000 "$FullTest"
test_component eq-iir 24 24 48000 "$FullTest"
test_component eq-iir 32 32 48000 "$FullTest"

# test with eq-fir
test_component eq-fir 32 32 48000 "$FullTest"

# test with dcblock
test_component dcblock 32 32 48000 "$FullTest"

# test with drc
test_component drc 32 32 48000 "$FullTest"

# test with multiband-drc
test_component multiband-drc 32 32 48000 "$FullTest"

# test with src
test_component src 24 24 48000 "$FullTest"

# test with tdfb
test_component tdfb 32 32 48000 "$FullTest"

echo "All tests are done!"
