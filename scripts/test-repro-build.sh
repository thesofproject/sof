#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation. All rights reserved.

# This builds SOF twice with some environment variations and then
# compares the binary outputs.

# Big caveats:
#
# - Only -g0 is supported, no BUILD_PATH_PREFIX_MAP
# - rimage uses the current date (day month year) in the
#   signature.
# - Starting from TGL, signatures also include a random salt.
#
# rimage makes sof.ri non-deterministic even when sof.elf is
# deterministic. See https://github.com/thesofproject/rimage/issues/41
# and below for more details and future directions.

# The following, basic environment variations are currently tested:
#
#   - timestamps (obviously)
#   - source directory
#   - disorderfs

# For a list of other, non-tested environment variations check the
# "Achieve deterministic builds" list found at
# https://reproducible-builds.org/docs/


set -e


SOF_TOP=$(cd "$(dirname "$0")"/.. && /bin/pwd)
SOF_PARENT=$(cd "$SOF_TOP"/.. && /bin/pwd)


SOF2="$SOF_PARENT"/sof-bind-mount-DO-NOT-DELETE
# To avoid ripple effects and reduce the number of differences
# considerably, replace "sof" with something of the same length:
# SOF2="$SOF_PARENT"/sog

PLATFS=(tgl)

# diffoscope is great but it has hundreds of dependencies, too long to
# install for CI so we don't use it here. This is just an alias
# suggestion for local use. Requires less -R.
diffo ()
{
    # Omitting $2 ("and") allows copy/paste of the output of diff -r
    diffoscope --exclude-directory-metadata=recursive \
               --text-color=always "$1" "$3"
}

hexdiff ()
{
    # cut -b10 removes the offset
    diff --color=auto -U20 \
         <(hexdump -vC "$1" | cut -b10- ) \
         <(hexdump -vC "$2" | cut -b10- ) | less
}

init()
{
    # -E(xtended) regex
    local excluded_endings=(
        '\.log' /deps
        '\.c?make' '\.ninja' /TargetDirectories.txt
        /CMakeCache.txt /progress.marks /CMakeRulesHashes.txt /Makefile
        /CMakeRuleHashes.txt /Makefile2
        '/depend\.internal' /link.txt '\.includecache'
        '/.ninja_deps' '/.ninja_log'
        '.*\.c\.o\.d'
        '/rimage_ep.*'
        '/smex_ep.*'
    )

    # The signing process is not deterministic.
    # Keep 'reproducible.ri' INCLUDED!
    excluded_endings+=('/sof-[a-z-]{,6}\.rix?')
    excluded_endings+=('/sof\.ri' '/sof-no-xman\.ri' )

    GREP_EXCLUDE_ARGS=()
    for fpath in "${excluded_endings[@]}"; do
        GREP_EXCLUDE_ARGS+=('-e' "$fpath differ\$")
    done

    # For debug
    # declare -p GREP_EXCLUDE_ARGS # exit
}


trap _unmount EXIT

_unmount()
{
    set +e

    if mount | grep -q "${SOF2}.*fuse"; then
        umount "$SOF2"
        rmdir "$SOF2"
    fi
}


diff_some_files()
{
    local p f
    ( set +e

      for p in "${PLATFS[@]}"; do

          # Compare some ELF section headers. Look at the smallest .c.o
          # first
          for f in CMakeFiles/sof.dir/src/arch/xtensa/init.c.o  sof; do

              diff --report-identical-files  \
                   {b0,b1}/build_"$p"_?cc/"$f" ||
                  diff -u --ignore-matching-lines='file format ' \
                       <(objdump -h b0/build_"$p"_?cc/"$f") \
                       <(objdump -h b1/build_"$p"_?cc/"$f")

          done

          for f in generated/include/sof_versions.h sof.ri \
                  src/arch/xtensa/reproducible.ri; do
              diff -u --report-identical-files  \
                   {b0,b1}/build_"$p"_?cc/"$f"
              # report unsupported reproducible.ri
              test -s b0/build_"$p"_?cc/"$f" ||
                  printf '\t%s is EMPTY\n' b0/build_"$p"_?cc/"$f"
          done

      done
      true
    ) | sed -e 's/differ/DIFFER/g'
}

main()
{
    init

    test -e "$SOF2"/CMakeLists.txt || {
        mkdir -p "$SOF2"
        # This will fail in Docker; Docker should provide this bind
        # mount for us.
        disorderfs "${SOF_TOP}" "$SOF2"
    }

    # Chances of name collisions with the user should be pretty small
    rm -rf   reprobld/b0/ reprobld/b1/
    mkdir -p reprobld/b0/ reprobld/b1/

    local oldTZ="$TZ" # typically undefined, coming from some /etc file
                      # instead.

    export EXTRA_CFLAGS='-g0'

    # Going once

    export TZ='Pacific/Midway' # -11:00
    "${SOF_TOP}"/scripts/xtensa-build-all.sh     "${PLATFS[@]}"
    mv build_*_?cc reprobld/b0/


    # Going twice

    # Use this to "break" rimage that makes the date (in local time!
    # BIOS fans?) part of the CSS signature. See caveats above.
    # +14:00 - -11:00 = +25:00 = always on a different date
    export TZ='Pacific/Kiritimati' # +14:00

    "$SOF2"/scripts/xtensa-build-all.sh "${PLATFS[@]}"
    mv build_*_?cc reprobld/b1/


    # Restore TZ just in case we want to use dates later.
    TZ="$oldTZ"


    cd reprobld
    diff -qr b0 b1/ |
        grep -E -v "${GREP_EXCLUDE_ARGS[@]}" || {
        printf \
          "\n\n ---- PASS: no unexpected difference found between %s/b0/ and b1/ --- \n\n" "$(pwd)"
        exit 0
    }

    printf "\n\n ---- FAIL: differences found between %s/b0/ and b1/ --- \n\n" "$(pwd)"
    diff_some_files
    printf "\n\n ---- FAIL: differences found between %s/b0/ and b1/ --- \n\n" "$(pwd)"
}

main "$@"
