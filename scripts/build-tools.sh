#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# fail immediately on any errors
set -e

SOF_TOP=$(cd "$(dirname "$0")/.." && pwd)

print_usage()
{
        cat <<EOFUSAGE
Deletes and re-builds from scratch CMake projects in the tools/
directory.
Attention: the list below is _not_ exhaustive. To re-build _everything_
from scratch don't select any particular target; this will build the
CMake's default target "ALL".

usage: $0 [-c|-f|-h|-l|-p|-t|-T|-A]
       -h Display help

       -c Rebuild ctl/
       -f Rebuild fuzzer/  # deprecated, see fuzzer/README.md
       -l Rebuild logger/
       -p Rebuild probes/
       -T Rebuild topology/ (not topology/development/! Use ALL)
       -t Rebuild test/topology/ (or tools/test/topology/tplg-build.sh directly)
       -A Clone and rebuild local ALSA lib and utils.

       -C No build, only CMake re-configuration. Shows CMake targets.
EOFUSAGE
}

# generate Makefiles
reconfigure_build()
{
        rm -rf "$BUILD_TOOLS_DIR"
        mkdir -p "$BUILD_TOOLS_DIR"

        ( cd "$BUILD_TOOLS_DIR"
          cmake -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" "${SOF_REPO}/tools"
        )

        mkdir "$BUILD_TOOLS_DIR/fuzzer"
        ( cd "$BUILD_TOOLS_DIR/fuzzer"
          cmake -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" "${SOF_REPO}/tools/fuzzer"
        )
}

make_tool()
{
        # if no argument provided, all the tools will be built. Empty tool is
        # okay.
        tool=$1

        ( set -x
        # shellcheck disable=SC2086
        cmake --build $BUILD_TOOLS_DIR  --  -j "$NO_PROCESSORS" $tool
        )
}

make_fuzzer()
{
        ( set -x
        cmake --build "$BUILD_TOOLS_DIR"/fuzzer  --  -j "$NO_PROCESSORS"
        )
}

print_build_info()
{
       cat <<EOFUSAGE

Build commands for respective tools:
        ctl:        make -C "$BUILD_TOOLS_DIR" sof-ctl
        logger:     make -C "$BUILD_TOOLS_DIR" sof-logger
        probes:     make -C "$BUILD_TOOLS_DIR" sof-probes
        topologies: make -C "$BUILD_TOOLS_DIR" topologies
        test tplgs: make -C "$BUILD_TOOLS_DIR" tests
               (or ./tools/test/topology/tplg-build.sh directly)

        fuzzer:     make -C "$BUILD_TOOLS_DIR/fuzzer"

        list of targets:
                    make -C "$BUILD_TOOLS_DIR/" help
EOFUSAGE
}

main()
{
        local DO_BUILD_ctl DO_BUILD_fuzzer DO_BUILD_logger DO_BUILD_probes \
                DO_BUILD_tests DO_BUILD_topologies SCRIPT_DIR SOF_REPO CMAKE_ONLY \
                BUILD_ALL
        SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
        SOF_REPO=$(dirname "$SCRIPT_DIR")
        : "${BUILD_TOOLS_DIR:=$SOF_REPO/tools/build_tools}"
        : "${NO_PROCESSORS:=$(nproc)}"
        BUILD_ALL=false

        if [ $# -eq 0 ]; then
                BUILD_ALL=true
        fi

        DO_BUILD_ctl=false
        DO_BUILD_fuzzer=false
        DO_BUILD_alsa=false
        DO_BUILD_logger=false
        DO_BUILD_probes=false
        DO_BUILD_tests=false
        DO_BUILD_topologies=false
        CMAKE_ONLY=false

        # eval is a sometimes necessary evil
        # shellcheck disable=SC2034
        while getopts "cfhlptTCA" OPTION; do
                case "$OPTION" in
                c) DO_BUILD_ctl=true ;;
                f) DO_BUILD_fuzzer=true ;;
                l) DO_BUILD_logger=true ;;
                p) DO_BUILD_probes=true ;;
                t) DO_BUILD_tests=true ;;
                T) DO_BUILD_topologies=true ;;
                C) CMAKE_ONLY=true ;;
                A) DO_BUILD_alsa=true ;;
                h) print_usage; exit 1;;
                *) print_usage; exit 1;;
                esac
        done
        shift "$((OPTIND - 1))"
        reconfigure_build

        if "$DO_BUILD_alsa"; then
                $SOF_TOP/scripts/build-alsa-tools.sh
        fi

        if "$CMAKE_ONLY"; then
                print_build_info
                exit
        fi

        if "$BUILD_ALL"; then
                # default CMake targets
                make_tool # trust set -e

                make_fuzzer
                exit $?
        fi

        # Keep 'topologies' first because it's the noisiest.
        for util in topologies tests; do
                if eval '$DO_BUILD_'$util; then
                        make_tool $util
                fi
        done

        for tool in ctl logger probes; do
                if eval '$DO_BUILD_'$tool; then
                        make_tool sof-$tool
                fi
        done

        if "$DO_BUILD_fuzzer"; then
                make_fuzzer
        fi
}

main "$@"
