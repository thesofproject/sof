#!/bin/bash
set -e

print_help()
{
    cat <<EOFHELP

Usage:

      $0 -b      -- -DOVERLAY_CONFIG=stub_build_all_ipc4.conf -DEXTRA_CFLAGS="-O0 -g3" ...
      $0 -t 500  -- -DOVERLAY_CONFIG=stub_build_all_ipc3.conf ...

  -p         Delete build-fuzz/ first ("pristine")
  -b         Do not run/fuzz: stop after the build.
  -t n       Fuzz for n seconds.
  -o ofile   Redirect the fuzzer's extremely verbose stdout. The
             relatively verbose stderr is not redirected by -o.
  -j n       Number of concurrent -jobs=n. Defaults to 1.
             The value 0 uses the output of the 'nproc' command.
  -d         Add debug optimizations and symbols to the build.
             Useful for testcase reproduction.

Arguments after -- are passed as is to CMake (through west).
When passing conflicting -DVAR='VAL UE1' -DVAR='VAL UE2' to CMake,
the last 'VAL UE2' wins; previous values are silently ignored.

The number of jobs defaults to 1 for (at least) two reasons:
- Other workers do not stop immediately after one worker fails.
- When higher than 1, this script has to unfortunately disable the
  (very verbose) stdout because libFuzzer mixes it with the (terse and
  useful) stderr.

Fuzzing happens to require stubbing which provides a great solution to
compile-check many CONFIG_* at once. So you can stop after the build
with the -b option.

Simple wrapper around a libfuzzer test run, as much for
documentation as direct use.  The idea here is really simple: build
for the Zephyr "native_sim" board (which is a just a x86
executable for the build host, not an emulated device) and run the
resulting zephyr.exe file.  This specifies a "fuzz_corpus" directory
to save the seeds that produce useful coverage output for use in
repeated runs (these are not particularly large, we might consider
curating and committing such a seed directory to the tree).

The tool will run until it finds a failure condition.  You will see
MANY errors on stdout from all the randomized input.  Don't try to
capture this, either redirect to a file with '-o fuzz.out' or arrange
to keep only the last XXX lines after the tool exits.

The only prerequisite to install is a clang compiler on the host.
Versions 12+ have all been observed to work.

You will need the kconfigs specified below for correct operation,
but can add more at the end of this script's command line to
duplicate configurations as needed.  Alternatively you can pass
overlay files in kconfig syntax via:

   fuzz.sh  -t 300 -- -DOVERLAY_CONFIG=... -DEXTRA_CFLAGS='-Wone -Wtwo' ...

EOFHELP
}


# To test this test, make the following change locally:
: <<EOF_TEST_PATCH
--- a/src/ipc/ipc3/handler.c
+++ b/src/ipc/ipc3/handler.c
@@ -1609,6 +1609,8 @@ void ipc_boot_complete_msg(struct ipc_cmd_hdr *header, uint32_t data)

 void ipc_cmd(struct ipc_cmd_hdr *_hdr)
 {
+       __ASSERT(false, "test the IPC3 fuzzer test");
+
        struct sof_ipc_cmd_hdr *hdr = ipc_from_hdr(_hdr);
EOF_TEST_PATCH

# When fuzzing IPC4, make the same change in src/ipc/ipc4/handler.c#ipc_cmd()

main()
{
  setup

  # Default values
  local JOBS=1
  local PRISTINE=false
  local BUILD_ONLY=false
  local FUZZER_STDOUT=/dev/stdout # bashism
  local TEST_DURATION=3
  local DEBUG_BUILD=false

  # Parse "$@". getopts stops after '--'
  while getopts "hj:po:t:bd" opt; do
      case "$opt" in
          h) print_help; exit 0;;
          j) if [ "$OPTARG" -eq 0 ]; then JOBS=$(nproc); else JOBS="$OPTARG"; fi;;
          p) PRISTINE=true;;
          o) FUZZER_STDOUT="$OPTARG";;
          t) TEST_DURATION="$OPTARG";;
          b) BUILD_ONLY=true;;
          d) DEBUG_BUILD=true;;
          *) print_help; exit 1;;
      esac
  done

  # Pass all "$@" arguments remaining after '--' to cmake.
  # This also drops _leading_ '--'.
  shift $((OPTIND-1))

  # Move this to a new fuzz.conf overlay file if it grows bigger
  local fuzz_configs=(
    -DCONFIG_ZEPHYR_POSIX=y
    -DCONFIG_ASSERT=y
    -DCONFIG_EXCEPTION_DEBUG=y
    -DCONFIG_ARCH_POSIX_TRAP_ON_FATAL=y
    -DCONFIG_SYS_HEAP_BIG_ONLY=y
    -DCONFIG_ZEPHYR_NATIVE_DRIVERS=y
    -DCONFIG_ARCH_POSIX_LIBFUZZER=y
    -DCONFIG_ZEPHYR_POSIX_FUZZ_TICKS=100
    -DCONFIG_ASAN=y
  )

  local debug_configs=(
    -DCONFIG_DEBUG=y
    -DCONFIG_DEBUG_OPTIMIZATIONS=y
  )

  # append debug flags if a debug build
  if $DEBUG_BUILD; then fuzz_configs=( "${fuzz_configs[@]}" "${debug_configs[@]}" ); fi

  # Note there's never any reason to delete fuzz_corpus/.
  # Don't trust `west build -p` because it is not 100% unreliable,
  # especially not when doing unusual toolchain things.
  if $PRISTINE; then rm -rf build-fuzz/; fi

  (set -x
   # When passing conflicting -DVAR='VAL UE1' -DVAR='VAL UE2' to CMake,
   # the last 'VAL UE2' wins. Previous ones are silently ignored.
  west build -d build-fuzz -b native_sim "$SOF_TOP"/app/ -- \
      "${fuzz_configs[@]}" "$@"
  )

  if $BUILD_ONLY; then
      exit 0
  fi

  mkdir -p ./fuzz_corpus

  local jobs_opts=( )
  # Do not use '-jobs=1' because it ignores FUZZER_STDOUT too
  if [ "$JOBS" -gt 1 ]; then
      # stdout and stderr get mixed and can't be separated anymore, so
      # kiss stdout goodbye.
      [ "$FUZZER_STDOUT" = /dev/stdout ] ||
          >&2 printf 'WARN: redirection to %s EMPTY when jobs > 1\n' "$FUZZER_STDOUT"
      jobs_opts+=( -jobs="$JOBS" -close_fd_mask=1 )
  fi

  date
  # Help is at: -help=1
  ( set -x
    >"$FUZZER_STDOUT" build-fuzz/zephyr/zephyr.exe -max_total_time="$TEST_DURATION" \
     -verbosity=0 "${jobs_opts[@]}" ./fuzz_corpus ) || {
      ret=$?
      >&2 printf 'zephyr.exe returned: %d\n' $ret
      date
      die "FAIL. To debug, run: gdb -tui ./build-fuzz/zephyr/zephyr.exe -ex 'run > _ ./crash-...'"
  }

  date
}


setup()
{
    SOF_TOP=$(cd "$(dirname "$0")/.." && pwd)
    export SOF_TOP

    export ZEPHYR_TOOLCHAIN_VARIANT=llvm

    # Define ZEPHYR_BASE so this can be invoked even outside the west workspace.
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
