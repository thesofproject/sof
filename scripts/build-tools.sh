#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# fail immediately on any errors
set -e

print_usage()
{
        cat <<EOFUSAGE
usage: $0 [-c|-f|-h|-l|-p|-t|-T]
       -c Rebuild ctl
       -f Rebuild fuzzer
       -h Display help
       -l Rebuild logger
       -p Rebuild probes
       -t Rebuild test topologies
       -T Rebuild topologies
       -C No build, only CMake re-configuration
EOFUSAGE
}

# generate Makefiles
reconfigure_build()
{
        rm -rf "$BUILD_TOOLS_DIR"
        mkdir "$BUILD_TOOLS_DIR"

        cd "$BUILD_TOOLS_DIR"
        cmake -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" ..

        mkdir "$BUILD_TOOLS_DIR/fuzzer"
        cd "$BUILD_TOOLS_DIR/fuzzer"
        cmake -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" ../../fuzzer
}

make_tool()
{
        # if no argument provided, all the tools will be built. Empty tool is
        # okay.
        tool=$1

        # shellcheck disable=SC2086
        cmake --build $BUILD_TOOLS_DIR  --  -j "$NO_PROCESSORS" $tool

}

make_fuzzer()
{
        cmake --build "$BUILD_TOOLS_DIR"/fuzzer  --  -j "$NO_PROCESSORS"
}

print_build_info()
{
       cat <<EOFUSAGE

Build commands for respective tools:
        ctl:        make -C "$BUILD_TOOLS_DIR" sof-ctl
        logger:     make -C "$BUILD_TOOLS_DIR" sof-logger
        probes:     make -C "$BUILD_TOOLS_DIR" sof-probes
        tests:      make -C "$BUILD_TOOLS_DIR" tests
        topologies: make -C "$BUILD_TOOLS_DIR" topologies
        fuzzer:     make -C "$BUILD_TOOLS_DIR/fuzzer"
EOFUSAGE
}

main()
{
        local DO_BUILD_ctl DO_BUILD_fuzzer DO_BUILD_logger DO_BUILD_probes \
                DO_BUILD_tests DO_BUILD_topologies SCRIPT_DIR SOF_REPO CMAKE_ONLY \
                BUILD_ALL
        SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
        SOF_REPO=$(dirname "$SCRIPT_DIR")
        BUILD_TOOLS_DIR="$SOF_REPO"/tools/build_tools
        : "${NO_PROCESSORS:=$(nproc)}"
        BUILD_ALL=false

        if [ $# -eq 0 ]; then
                BUILD_ALL=true
        fi

        DO_BUILD_ctl=false
        DO_BUILD_fuzzer=false
        DO_BUILD_logger=false
        DO_BUILD_probes=false
        DO_BUILD_tests=false
        DO_BUILD_topologies=false
        CMAKE_ONLY=false

        # eval is a sometimes necessary evil
        # shellcheck disable=SC2034
        while getopts "cfhlptTC" OPTION; do
                case "$OPTION" in
                c) DO_BUILD_ctl=true ;;
                f) DO_BUILD_fuzzer=true ;;
                l) DO_BUILD_logger=true ;;
                p) DO_BUILD_probes=true ;;
                t) DO_BUILD_tests=true ;;
                T) DO_BUILD_topologies=true ;;
                C) CMAKE_ONLY=true ;;
                h) print_usage; exit 1;;
                *) print_usage; exit 1;;
                esac
        done
        shift "$((OPTIND - 1))"
        reconfigure_build

        if "$CMAKE_ONLY"; then
                print_build_info
                exit
        fi

        if "$BUILD_ALL"; then
                make_tool
                make_fuzzer
                exit
        fi

        for tool in ctl logger probes; do
                if eval '$DO_BUILD_'$tool; then
                        make_tool sof-$tool
                fi
        done

        for util in tests topologies; do
                if eval '$DO_BUILD_'$util; then
                        make_tool $util
                fi
        done

        if "$DO_BUILD_fuzzer"; then
                make_fuzzer
        fi
}

main "$@"
