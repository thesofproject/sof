#!/bin/bash
set -e

print_help()
{
    cat <<EOFHELP
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
# overlay files in kconfig syntax via:
#    fuzz.sh  -t 300 -- -DOVERLAY_CONFIG=..., etc...
EOFHELP
}


# To test this test, make the following change locally:
: <<EOF_TEST_PATCH
--- a/src/ipc/ipc3/handler.c
+++ b/src/ipc/ipc3/handler.c
@@ -1609,6 +1609,8 @@ void ipc_boot_complete_msg(struct ipc_cmd_hdr *header, uint32_t data)

 void ipc_cmd(struct ipc_cmd_hdr *_hdr)
 {
+       __ASSERT(false, "test the fuzzer test");
+
        struct sof_ipc_cmd_hdr *hdr = ipc_from_hdr(_hdr);
EOF_TEST_PATCH


main()
{
  setup

  # Parse "$@". getopts stops after '--'
  while getopts "ho:t:" opt; do
      case "$opt" in
          h) print_help; exit 0;;
          o) FUZZER_STDOUT="$OPTARG";;
          t) TEST_DURATION="$OPTARG";;
          *) print_help; exit 1;;
      esac
  done

  # Pass all "$@" arguments remaining after '--' to cmake.
  # This also drops _leading_ '--'.
  shift $((OPTIND-1))

  # Default values
  : "${TEST_DURATION:=3}"
  : "${FUZZER_STDOUT:=/dev/stdout}" # bashism

  west build -d build-fuzz -b native_posix "$SOF_TOP"/app/ -- \
    -DCONFIG_ASSERT=y \
    -DCONFIG_SYS_HEAP_BIG_ONLY=y \
    -DCONFIG_ZEPHYR_NATIVE_DRIVERS=y \
    -DCONFIG_ARCH_POSIX_LIBFUZZER=y \
    -DCONFIG_ARCH_POSIX_FUZZ_TICKS=100 \
    -DCONFIG_ASAN=y "$@"

  mkdir -p ./fuzz_corpus

  timeoutret=0
  date
  timeout -k 10 "$TEST_DURATION" >"$FUZZER_STDOUT" build-fuzz/zephyr/zephyr.exe ./fuzz_corpus ||
      timeoutret=$?
  if [ $timeoutret -eq 124 ]; then
      # The current ASAN seems overly sensitive: it frequently (but not always) complains
      # loudly that it has been aborted with a so-called DEADLYSIGNAL when the timeout
      # command above actually delivers just a gentle TERM by default. Counteracts the
      # wrong impression this unpredictable whining can give with a proud "successfully"
      # message.
      printf '\n\nSuccessfully aborted fuzzer after %d seconds test duration with DEADLYSIGNAL\n' \
             "$TEST_DURATION"
  else
      die 'zephyr.exe fuzzer stopped before timing out: %d\n' $timeoutret
  fi
  date
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

die()
{
        >&2 printf '%s ERROR: ' "$0"
        # We want die() to be usable exactly like printf
        # shellcheck disable=SC2059
        >&2 printf "$@"
        >&2 printf '\n'
        exit 1
}

main "$@"
