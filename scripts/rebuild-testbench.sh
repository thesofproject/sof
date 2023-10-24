#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020, Mohana Datta Yelugoti

# stop on most errors
set -e

# Defaults
BUILD_TYPE=native
BUILD_DIR_NAME=build_testbench
BUILD_TARGET=install

print_usage()
{
    cat <<EOFUSAGE
usage: $0 [-f] [-p <platform>]
       -p Build testbench binary for xt-run for selected platform, e.g. -p tgl
          When omitted, perform a BUILD_TYPE=native, compile-only check.
       -f Build testbench with compiler provided by fuzzer
          (default path: $HOME/sof/work/AFL/afl-gcc)
       -j number of parallel make/ninja jobs. Defaults to /usr/bin/nproc.
          You MUST re-run with -j1 when something is failing!
EOFUSAGE
}

# die is copy from xtensa-build-all.sh
die()
{
    >&2 printf '%s ERROR: ' "$0"
    # We want die() to be usable exactly like printf
    # shellcheck disable=SC2059
    >&2 printf "$@"
    exit 1
}

rebuild_testbench()
{
    cd "$BUILD_TESTBENCH_DIR"

    rm -rf "$BUILD_DIR_NAME"
    mkdir "$BUILD_DIR_NAME"
    cd "$BUILD_DIR_NAME"

    cmake -DCMAKE_INSTALL_PREFIX=install  ..

    cmake --build .  --  -j"${jobs}" $BUILD_TARGET
}

export_CC_with_afl()
{
    printf 'export CC=%s\n' "${SOF_AFL}"
    export CC=${SOF_AFL}
}

setup_xtensa_tools_build()
{
    BUILD_TYPE=xt
    BUILD_TARGET=
    BUILD_DIR_NAME=build_xt_testbench
    COMPILER="xt-xcc"

    # check needed environment variables
    test -n "${XTENSA_TOOLS_ROOT}" || die "XTENSA_TOOLS_ROOT need to be set.\n"

    # Get compiler version for platform

    # 'shellcheck -x ...' works only from the top directory
    # shellcheck source=scripts/set_xtensa_params.sh
    source "$SCRIPT_DIR/set_xtensa_params.sh" "$BUILD_PLATFORM"

    test -n "${XTENSA_TOOLS_VERSION}" ||
        die "Illegal platform $BUILD_PLATFORM, no XTENSA_TOOLS_VERSION found.\n"
    test -n "${XTENSA_CORE}" ||
        die "Illegal platform $BUILD_PLATFORM, no XTENSA_CORE found.\n"

    install_bin=install/tools/$XTENSA_TOOLS_VERSION/XtensaTools/bin
    tools_bin=$XTENSA_TOOLS_ROOT/$install_bin
    testbench_sections="-Wl,--sections-placement $BUILD_TESTBENCH_DIR/testbench_xcc_sections.txt"
    export CC=$tools_bin/$COMPILER
    export LD=$tools_bin/xt-ld
    export OBJDUMP=$tools_bin/xt-objdump
    export LDFLAGS="-mlsp=sim $testbench_sections"
    export XTENSA_CORE
}

export_xtensa_setup()
{
    export_dir=$BUILD_TESTBENCH_DIR/$BUILD_DIR_NAME
    export_script=$export_dir/xtrun_env.sh
    xtbench=$export_dir/testbench
    xtbench_run="XTENSA_CORE=$XTENSA_CORE \$XTENSA_TOOLS_ROOT/$install_bin/xt-run $xtbench"
    cat <<EOFSETUP > "$export_script"
export XTENSA_TOOLS_ROOT=$XTENSA_TOOLS_ROOT
export XTENSA_CORE=$XTENSA_CORE
XTENSA_PATH=$tools_bin
EOFSETUP
}

testbench_usage()
{
    case "$BUILD_TYPE" in
        xt)
            export_xtensa_setup
            cat <<EOFUSAGE
Success! Testbench binary for $BUILD_PLATFORM is in $xtbench
it can be run with command:

$xtbench_run -h

Alternatively with environment setup to match build:

source $export_script
\$XTENSA_PATH/xt-run $xtbench -h

EOFUSAGE
            ;;
        *) >&2 printf 'testbench_usage: unknown/missing BUILD_TYPE=%s\n' "$BUILD_TYPE" ;;
    esac
}

main()
{
    SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
    SOF_REPO=$(dirname "$SCRIPT_DIR")
    BUILD_TESTBENCH_DIR="$SOF_REPO"/tools/testbench
    : "${SOF_AFL:=$HOME/sof/work/AFL/afl-gcc}"

    jobs=$(nproc)
    while getopts "fhj:p:" OPTION; do
        case "$OPTION" in
            p)
                BUILD_PLATFORM="$OPTARG"
                setup_xtensa_tools_build
                ;;
            f) export_CC_with_afl;;
            j) jobs=$(printf '%d' "$OPTARG") ;;
            h) print_usage; exit 1;;
            *) print_usage; exit 1;;
        esac
    done

    rebuild_testbench
    printf '\n'
    testbench_usage
}

main "$@"
