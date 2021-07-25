#!/bin/sh

set -e

SOFTOP=$(cd "$(dirname "$0")"/.. && pwd)

BUILDTOP=build_ut_defs

rebuild_config()
{
    local conf="$1"
    mkdir -p "$BUILDTOP"
    ( set -x
     cmake  -DINIT_CONFIG="$conf" -B "$BUILDTOP"/"$conf" -DBUILD_UNIT_TESTS=ON \
           -DBUILD_UNIT_TESTS_HOST=ON

     cmake --build "$BUILDTOP"/"$conf" -- -j"$(nproc)"
    )
}

main()
{
    local defconfig

    # First make sure all configurations build
    for d in "$SOFTOP"/src/arch/xtensa/configs/*_defconfig; do
        defconfig=$(basename "$d")
        rebuild_config "$defconfig"
    done

    # Now run all the tests
    for d in "$BUILDTOP"/*_defconfig; do
        ( set -x
         cmake --build "$d" -- test
        )
    done
}

main "$@"
