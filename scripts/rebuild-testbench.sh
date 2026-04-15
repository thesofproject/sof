#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020, Mohana Datta Yelugoti

# stop on most errors
set -e

# This script can be executed from the sof firmware toplevel or workspace directory
# First check for the worksapce environment variable
if [ -z "$SOF_WORKSPACE" ]; then
    # Environment variable is empty or unset so use default
    BASE_DIR="$HOME/work/sof"
else
    # Environment variable exists and has a value
    BASE_DIR="$SOF_WORKSPACE"
fi
cd "$BASE_DIR"

# check we are in the workspace directory
if [ ! -d "sof" ]; then
    echo "Error: can't find SOF firmware directory. Please check your installation."
    exit 1
fi

TESTBENCH_DIR=$BASE_DIR/sof/tools/testbench

# Defaults
BUILD_BACKEND='make'
BUILD_TYPE=native
BUILD_DIR_NAME=$BASE_DIR/build_testbench
BUILD_TARGET=install
: "${SOF_AFL:=$HOME/sof/work/AFL/afl-gcc}"
SCRIPT_DIR=$BASE_DIR/sof/scripts

print_usage()
{
    cat <<EOFUSAGE
usage: $0 [-f] [-p <platform>]
       -p Build testbench binary for xt-run for selected platform, e.g. -p tgl
          When omitted, perform a BUILD_TYPE=native, compile-only check.
       -f Build testbench with compiler provided by fuzzer
          (default path: $HOME/sof/work/AFL/afl-gcc)
       -j number of parallel $BUILD_BACKEND jobs. Defaults to /usr/bin/nproc.
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
    local bdir="$BUILD_DIR_NAME"
    cd "$TESTBENCH_DIR"

    rm -rf "$bdir"
    cmake -B "$bdir" -DCMAKE_INSTALL_PREFIX="$bdir"/install
    cmake --build "$bdir"  --  -j"${jobs}" $BUILD_TARGET
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
    BUILD_DIR_NAME=$BASE_DIR/build_xt_testbench

    # check needed environment variables
    test -n "${XTENSA_TOOLS_ROOT}" || die "XTENSA_TOOLS_ROOT need to be set.\n"

    # Get compiler version for platform

    # 'shellcheck -x ...' works only from the top directory
    # shellcheck source=scripts/set_xtensa_params.sh
    source "$SCRIPT_DIR/set_xtensa_params.sh" "$BUILD_PLATFORM"

    test -n "${TOOLCHAIN_VER}" ||
        die "Illegal platform $BUILD_PLATFORM, no TOOLCHAIN_VER found.\n"
    test -n "${XTENSA_CORE}" ||
        die "Illegal platform $BUILD_PLATFORM, no XTENSA_CORE found.\n"

    # This (local?) variable should probably be inlined
    COMPILER=xt-"$SOF_CC_BASE"

    install_bin=install/tools/$TOOLCHAIN_VER/XtensaTools/bin
    tools_bin=$XTENSA_TOOLS_ROOT/$install_bin
    testbench_sections="-Wl,--sections-placement $TESTBENCH_DIR/testbench_xcc_sections.txt"
    export CC=$tools_bin/$COMPILER
    export LD=$tools_bin/xt-ld
    export OBJDUMP=$tools_bin/xt-objdump
    export LDFLAGS="-mlsp=sim $testbench_sections"
    export XTENSA_CORE
}

export_xtensa_setup()
{
    export_dir=$BUILD_DIR_NAME
    export_script=$export_dir/xtrun_env.sh
    xtbench=$export_dir/sof-testbench4
    xtbench_run="XTENSA_CORE=$XTENSA_CORE \$XTENSA_TOOLS_ROOT/$install_bin/xt-run $xtbench"
    cat <<EOFSETUP > "$export_script"
export XTENSA_TOOLS_ROOT=$XTENSA_TOOLS_ROOT
export XTENSA_CORE=$XTENSA_CORE
XTENSA_PATH=$tools_bin
EOFSETUP
}

testbench_usage()
{
    local src_env_msg
    if [ "$BUILD_TYPE" = 'xt' ]; then
        export_xtensa_setup
        src_env_msg="source $export_script"
    fi

cat <<EOF0
Success!

For temporary, interactive Kconfiguration use:

   $BUILD_BACKEND -C $BUILD_DIR_NAME/sof_ep/build/  menuconfig

Permanent configuration is "src/arch/host/configs/library_defconfig".

For instant, incremental build:

   $src_env_msg
   $BUILD_BACKEND -C $BUILD_DIR_NAME/ -j$(nproc)

EOF0

    case "$BUILD_TYPE" in
        xt)
            cat <<EOFUSAGE
Testbench binary for $BUILD_PLATFORM is in $xtbench
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
    jobs=$(nproc)
    while getopts "fhj:p:" OPTION; do
        case "$OPTION" in
            p)
                BUILD_PLATFORM="$OPTARG"
                setup_xtensa_tools_build
                ;;
            f) export_CC_with_afl;;
            j) jobs=$(printf '%d' "$OPTARG") ;;
            h) print_usage; exit 0;;
            *) print_usage; exit 1;;
        esac
    done

    # This automagically removes the -- sentinel itself if any.
    shift "$((OPTIND -1))"
    # Error on spurious arguments.
    test $# -eq 0 || {
        print_usage
        die "Unknown arguments: %s\n" "$*"
    }

    rebuild_testbench

    printf '\n'
    testbench_usage
}

main "$@"
