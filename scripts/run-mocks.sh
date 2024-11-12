#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# Stop on most errors
set -e

SOFTOP=$(cd "$(dirname "$0")"/.. && pwd)

usage()
{
    cat <<EOF
Usage: [ -v ] [ -c platform_defconfig ]

Re-compiles unit tests with the host toolchain and runs them one by
one, optionally with valgrind. If you don't need valgrind it's faster
and better looking to run "make -j test". See
https://thesofproject.github.io/latest/developer_guides/unit_tests.html
EOF
    exit 1
}

parse_opts()
{
	# default config if none supplied
	CONFIG="unit_test_defconfig"
	VALGRIND_CMD=""

	while getopts "vc:" flag; do
	    case "${flag}" in
		c) CONFIG=${OPTARG}
		   ;;
		v) VALGRIND_CMD="valgrind --tool=memcheck --track-origins=yes \
--leak-check=full --show-leak-kinds=all --error-exitcode=1"
		   ;;
		?) usage
		;;
	    esac
	done

	shift $((OPTIND -1 ))
	# anything left?
	test -z "$1" || usage
}

rebuild_ut()
{
        # -DINIT_CONFIG is ignored after the first time. Invoke make (or
        # -ninja) after this for faster, incremental builds.
	rm -rf build_ut/

	cmake -S "$SOFTOP" -B build_ut -DBUILD_UNIT_TESTS=ON -DBUILD_UNIT_TESTS_HOST=ON \
	      -DINIT_CONFIG="${CONFIG}"

	cmake --build build_ut -- -j"$(nproc --all)"
}

run_ut()
{
	local TESTS; TESTS=$(find build_ut/test -type f -executable -print)
	echo test are "${TESTS}"
	for test in ${TESTS}
	do
	    if [ x"$test" = x'build_ut/test/cmocka/src/lib/alloc/alloc' ]; then
		printf 'SKIP alloc test until it is fixed on HOST (passes with xt-run)'
		continue
	    fi

	    printf 'Running %s\n' "${test}"
	    ( set -x
	      ${VALGRIND_CMD} ./"${test}"
	    )
	done
}

main()
{
	parse_opts "$@"
	rebuild_ut
	run_ut
}

main "$@"
