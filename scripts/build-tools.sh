#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# fail immediately on any errors
set -e

SOF_TOP=$(cd "$(dirname "$0")/.." && pwd)

print_usage()
{
        cat <<EOFUSAGE

Configures and builds selected CMake projects in the tools/ directory.
Attention: the list of selected shortcuts below is _not_ exhaustive. To
build _everything_ don't select any particular target; this will build
CMake's default target "ALL".

usage: $0 [-c|-f|-h|-l|-p|-t|-T|-X|-Y]
       -h Display help

       -c Rebuild ctl/
       -l Rebuild logger/
       -p Rebuild probes/
       -T Rebuild topology/ (not topology/development/! Use ALL)
       -X Rebuild topology1 only
       -Y Rebuild topology2 only
       -t Rebuild test/topology/ (or tools/test/topology/tplg-build.sh directly)

       -C No build, only CMake re-configuration. Shows CMake targets.
EOFUSAGE

        warn_if_incremental_build
}

# generate Makefiles
reconfigure_build()
{
        rm -rf "$BUILD_TOOLS_DIR"
        mkdir -p "$BUILD_TOOLS_DIR"

        ( cd "$BUILD_TOOLS_DIR"
          cmake -GNinja -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" "${SOF_REPO}/tools"
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

print_build_info()
{
       cat <<EOFUSAGE

Build commands for respective tools:
        ctl:        ninja -C "$BUILD_TOOLS_DIR" sof-ctl
        logger:     ninja -C "$BUILD_TOOLS_DIR" sof-logger
        probes:     ninja -C "$BUILD_TOOLS_DIR" sof-probes
        topologies: ninja -C "$BUILD_TOOLS_DIR" topologies
        topologies1: ninja -C "$BUILD_TOOLS_DIR" topologies1
        topologies2: ninja -C "$BUILD_TOOLS_DIR" topologies2

        test tplgs: ninja -C "$BUILD_TOOLS_DIR" tests
               (or ./tools/test/topology/tplg-build.sh directly)

        list of targets:
                    ninja -C "$BUILD_TOOLS_DIR/" help
EOFUSAGE

       warn_if_incremental_build
}

warn_if_incremental_build()
{
        $warn_incremental_build || return 0
        cat <<EOF

WARNING: building tools/ is now incremental by default!
         To build from scratch delete: $BUILD_TOOLS_DIR
         or use the -C option.

EOF
}

main()
{
        local DO_BUILD_ctl DO_BUILD_logger DO_BUILD_probes \
                DO_BUILD_tests DO_BUILD_topologies1 DO_BUILD_topologies2 SCRIPT_DIR SOF_REPO \
                CMAKE_ONLY BUILD_ALL
        SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
        SOF_REPO=$(dirname "$SCRIPT_DIR")
        : "${BUILD_TOOLS_DIR:=$SOF_REPO/tools/build_tools}"
        : "${NO_PROCESSORS:=$(nproc)}"
        BUILD_ALL=false

        if [ $# -eq 0 ]; then
                BUILD_ALL=true
        fi

        DO_BUILD_ctl=false
        DO_BUILD_logger=false
        DO_BUILD_probes=false
        DO_BUILD_tests=false
        DO_BUILD_topologies1=false
        DO_BUILD_topologies2=false
        CMAKE_ONLY=false

        # better safe than sorry
        local warn_incremental_build=true

        # eval is a sometimes necessary evil
        # shellcheck disable=SC2034
        while getopts "cfhlptTCXY" OPTION; do
                case "$OPTION" in
                c) DO_BUILD_ctl=true ;;
                l) DO_BUILD_logger=true ;;
                p) DO_BUILD_probes=true ;;
                t) DO_BUILD_tests=true ;;
                T) DO_BUILD_topologies1=true ; DO_BUILD_topologies2=true ;;
                X) DO_BUILD_topologies1=true ;;
                Y) DO_BUILD_topologies2=true ;;
                C) CMAKE_ONLY=true ;;
                h) print_usage; exit 1;;
                *) print_usage; exit 1;;
                esac
        done
        shift "$((OPTIND - 1))"

        if "$CMAKE_ONLY"; then
                reconfigure_build
                print_build_info
                exit
        fi

        test -e "$BUILD_TOOLS_DIR"/build.ninja ||
        test -e "$BUILD_TOOLS_DIR"/Makefile    || {
            warn_incremental_build=false
            reconfigure_build
        }

        if "$BUILD_ALL"; then
                # default CMake targets
                make_tool # trust set -e

                warn_if_incremental_build
                exit 0
        fi

        # Keep 'topologies' first because it's the noisiest.
        for util in topologies1 topologies2 tests; do
                if eval '$DO_BUILD_'$util; then
                        make_tool $util
                fi
        done

        for tool in ctl logger probes; do
                if eval '$DO_BUILD_'$tool; then
                        make_tool sof-$tool
                fi
        done

        warn_if_incremental_build
}

main "$@"
