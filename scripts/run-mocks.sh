#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation. All rights reserved.

# fail on any errors
set -e

# default config of none supplied
CONFIG="tgph_defconfig"
VALGRIND_CMD=""

while getopts "vc:" flag; do
    case "${flag}" in
        c) CONFIG=${OPTARG};;
        v) VALGRIND_CMD="valgrind --tool=memcheck --track-origins=yes --leak-check=full --show-leak-kinds=all";;
        ?) echo "Usage: -v -c defconfig"
           exit 1;;
    esac
done



# clean build area
rm -rf build_ut
mkdir build_ut

# copy initial defconfig
cp src/arch/xtensa/configs/${CONFIG} initial.config
cd build_ut

cmake -DBUILD_UNIT_TESTS=ON -DBUILD_UNIT_TESTS_HOST=ON ..

make -j$(nproc --all)

TESTS=`find test -type f -executable -print`
echo test are ${TESTS}
for test in ${TESTS}
do
	echo got ${test}
	${VALGRIND_CMD} ./${test}
done
