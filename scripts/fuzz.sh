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

export SOF_TOP=$(cd "$(dirname "$0")/.." && pwd)

export ZEPHYR_BASE=$SOF_TOP/../zephyr
export ZEPHYR_TOOLCHAIN_VARIANT=llvm

main()
{
  west build -p -b native_posix $SOF_TOP/app/ -- \
    -DCONFIG_ASSERT=y \
    -DCONFIG_SYS_HEAP_BIG_ONLY=y \
    -DCONFIG_ZEPHYR_NATIVE_DRIVERS=y \
    -DCONFIG_ARCH_POSIX_LIBFUZZER=y \
    -DCONFIG_ARCH_POSIX_FUZZ_TICKS=100 \
    -DCONFIG_ASAN=y "$@"

  mkdir -p ./fuzz_corpus
  build/zephyr/zephyr.exe ./fuzz_corpus
}

main "$@"
