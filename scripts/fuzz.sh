#!/bin/bash
set -e

# Simple wrapper around a libfuzzer test run, as much for
# documentation as direct use.  The idea here is really simple: build
# for the Zephyr "native_posix" board (which is a just a x86
# executable for the build host, not an emulated device) and run the
# resulting zephyr.exe file.  This specifies a "fuzz_corpus" directory
# to save the seeds that produce useful coverage output for use in
# repeated runs (these are not particularly large, we might consider
# curating and commiting such a seed directory to the tree).
#
# The tool will run until it finds a failure condition.  You will see
# MANY errors on stdout from all the randomized input.  Don't try to
# capture this, either let it output to a terminal or arrange to keep
# only the last XXX lines after the tool exits.
#
# The only prerequisite to install is a clang compiler on the host.
# Versions 12+ have all been observed to work.
#
# You will need the kconfigs specified below for correct operation,
# but can add more at the end of this script's command line to
# duplicate configurations as needed.  Alternatively you can pass
# overlay files in kconfig syntax via -DOVERLAY_CONFIG=..., etc...

main()
{
  setup

  west build -p -d build-fuzz -b native_posix "$SOF_TOP"/app/ -- \
    -DCONFIG_ASSERT=y \
    -DCONFIG_SYS_HEAP_BIG_ONLY=y \
    -DCONFIG_ZEPHYR_NATIVE_DRIVERS=y \
    -DCONFIG_ARCH_POSIX_LIBFUZZER=y \
    -DCONFIG_ARCH_POSIX_FUZZ_TICKS=100 \
    -DCONFIG_ASAN=y "$@"

  mkdir -p ./fuzz_corpus
  build-fuzz/zephyr/zephyr.exe ./fuzz_corpus
}


setup()
{
    SOF_TOP=$(cd "$(dirname "$0")/.." && pwd)
    export SOF_TOP

    export ZEPHYR_TOOLCHAIN_VARIANT=llvm

    # ZEPHYR_BASE. Does not seem required.
    local WS_TOP
    WS_TOP=$(cd "$SOF_TOP"; west topdir)
    # The manifest itself is not a west project
    ZEP_DIR=$(2>/dev/null west list -f '{path}' zephyr || echo 'zephyr')
    export ZEPHYR_BASE="$WS_TOP/$ZEP_DIR"

}

main "$@"
