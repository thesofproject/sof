#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

#
# Usage:
# please run following scrits for the test tplg and host build
# ./scripts/build-tools.sh -t
# ./scripts/host-build-all.sh
# Run test
# ./scripts/host-testbench.sh
#

function filesize() {
  du -b $1 | awk '{print $1}'
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
  echo $OUTPUT_SIZE
}

SCRIPTS_DIR=$(dirname ${BASH_SOURCE[0]})
SOF_DIR=$SCRIPTS_DIR/../
TESTBENCH_DIR=${SOF_DIR}/tools/test/audio
INPUT_FILE_SIZE=10240

cd $TESTBENCH_DIR
rm -rf *.raw

# create input zeros raw file
head -c ${INPUT_FILE_SIZE} < /dev/zero > zeros_in.raw

# test with volume
echo "=========================================================="
echo "test volume with ./volume_run.sh 16 16 48000 zeros_in.raw volume_out.raw"
./volume_run.sh 16 16 48000 zeros_in.raw volume_out.raw &>vol.log
# VOL_SIZE=$(filesize volume_out.raw)
# echo "VOL_SIZE is $VOL_SIZE"
comparesize ${INPUT_FILE_SIZE} $(filesize volume_out.raw)
if [[ $? -ne 0 ]]; then
  echo "volume test failed!"
  cat vol.log
else
  echo "volume test passed!"
fi

# # test with src
echo "=========================================================="
echo "test src with ./src_run.sh 32 32 44100 48000 zeros_in.raw src_out.raw"
./src_run.sh 32 32 44100 48000 zeros_in.raw src_out.raw &>src.log
comparesize $(srcsize ${INPUT_FILE_SIZE} 44100 48000) $(filesize src_out.raw)
if [[ $? -ne 0 ]]; then
  echo "src test failed!"
  cat src.log
else
  echo "src test passed!"
fi

# test with eq
echo "=========================================================="
echo "test eqiir with ./eqiir_run.sh 16 16 48000 zeros_in.raw eqiir_out.raw"
./eqiir_run.sh 16 16 48000 zeros_in.raw eqiir_out.raw &>eqiir.log
comparesize $INPUT_FILE_SIZE $(filesize volume_out.raw)
if [[ $? -ne 0 ]]; then
  echo "eqiir test failed!"
  cat eqiir.log
else
  echo "eqiir test passed!"
fi
