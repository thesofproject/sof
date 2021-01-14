#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020, Mohana Datta Yelugoti

# fail on any errors
set -e

print_usage()
{
    cat <<EOFUSAGE
usage: $0 [-f]
       -f Build testbench with compiler provided by fuzzer
          (default path: $HOME/sof/work/AFL/afl-gcc)
EOFUSAGE
}

rebuild_testbench()
{
    cd "$BUILD_TESTBENCH_DIR"

    rm -rf build_testbench

    mkdir build_testbench
    cd build_testbench

    cmake -DCMAKE_INSTALL_PREFIX=install  ..

    cmake --build .  --  -j"$(nproc)" install
}

export_CC_with_afl()
{
    printf 'export CC=%s\n' "${SOF_AFL}"
    export CC=${SOF_AFL}
}

main()
{
    SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
    SOF_REPO=$(dirname "$SCRIPT_DIR")
    BUILD_TESTBENCH_DIR="$SOF_REPO"/tools/testbench
    : "${SOF_AFL:=$HOME/sof/work/AFL/afl-gcc}"

    while getopts "fh" OPTION; do
	case "$OPTION" in
	    f) export_CC_with_afl;;
	    h) print_usage; exit 1;;
	    *) print_usage; exit 1;;
	esac
    done

    rebuild_testbench
}

main "$@"
