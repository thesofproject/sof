#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# fail immediately on any errors
set -e


print_usage()
{
        cat <<EOFUSAGE
usage: $0 [-t|-f]
       [-t] Build test topologies
       [-f] Build fuzzer"
EOFUSAGE
}

build_tools()
{
        cd "$SOF_REPO/tools"
        rm -rf build_tools
        mkdir build_tools
        cd build_tools
        cmake ..
        make -j "$NO_PROCESSORS"
}

build_test()
{
        cd "$SOF_REPO/tools/build_tools"
        make tests -j "$NO_PROCESSORS"
}

build_fuzzer()
{
        cd "$SOF_REPO/tools/build_tools"
        mkdir build_fuzzer
        cd build_fuzzer
        cmake ../../fuzzer
        make -j "$NO_PROCESSORS"
}

main()
{
        local DO_BUILD_TEST DO_BUILD_FUZZER SCRIPT_DIR SOF_REPO NO_PROCESSORS

        SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
        SOF_REPO=$(dirname "$SCRIPT_DIR")
        NO_PROCESSORS=$(nproc)

        DO_BUILD_TEST=false
        DO_BUILD_FUZZER=false
        while getopts "tf" OPTION; do
                case "$OPTION" in
                t) DO_BUILD_TEST=true ;;
                f) DO_BUILD_FUZZER=true ;;
                *) print_usage; exit 1;;
                esac
        done
        shift "$(($OPTIND - 1))"

        build_tools

        if "$DO_BUILD_TEST"
        then
                build_test
        fi

        if "$DO_BUILD_FUZZER"
        then
                build_fuzzer
        fi
}

main "$@"
