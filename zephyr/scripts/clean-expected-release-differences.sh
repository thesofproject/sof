#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# shellcheck disable=SC3043

set -e

die()
{
    # shellcheck disable=SC2059
    >&2 printf "$@"
    exit 1
}

fix_dir()
{
    local bd="$1"

    test -d "$bd"/build-sof-staging ||
        die 'No %s/build-sof-staging directory\n' "$bd"

    # config files have absolute paths
    find "$bd" -name 'config.gz' -exec rm '{}' \;

    # In case of a compression timestamp. Also gives better messages.
    find "$bd" -name '*.gz' -print0 | xargs -r -0 gunzip

    ( set -x

      # Native binaries
      rm -f "$bd"/build-sof-staging/tools/sof-logger*
      # Python and other scripts
      dos2unix "$bd"/build-sof-staging/tools/* || true

      # signature salt
      find "$bd" -name '*.ri' -exec rm '{}' \;

      # debug symbols
      find "$bd" -name main.mod   -exec rm '{}' \;
      find "$bd" -name zephyr.elf -exec rm '{}' \;

      # Unlike zephyr.lst, zephyr.map includes some debug information which is
      # as usual full of absolute paths, e.g.:
      #  /opt/toolchains/zephyr-sdk-0.15.2/xtensa-intel_s1000_..../libgcc.a(_divsf3.o)
      # Delete non-reproducible information inside zephyr.map.
      find "$bd" -name zephyr.map -exec sed -i'' -e \
        's#[^[:blank:]]*zephyr-sdk-[^/]*/xtensa#ZSDK/xtensa#; s#\\#/#g; /^ \.debug_/ d' \
        '{}' \;

      # The above search/replace normalizes MOST but unfortunately not
      # all the debug information! So let's delete zephyr.map after all :-(
      # Comparing "almost normalized" zephyr.map files can be very
      # useful to root cause differences: comment out this line in your
      # local workspace.
      find "$bd" -name zephyr.map -exec rm '{}' \;

      find "$bd" -name 'compile_commands.json' -exec rm '{}' \;
    )

}

main()
{
    for d in "$@"; do
        fix_dir "$d"
    done
}


main "$@"
