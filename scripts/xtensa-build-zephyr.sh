#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation. All rights reserved.

# stop on most errors
set -e

SUPPORTED_PLATFORMS=(apl cnl icl tgl-h)
# By default use "zephyr" in the current directory
ZEPHYR_ROOT=zephyr
BUILD_JOBS=$(nproc --all)
RIMAGE_KEY=modules/audio/sof/keys/otc_private_key.pem
PLATFORMS=()

die()
{
	>&2 printf '%s ERROR: ' "$0"
	# We want die() to be usable exactly like printf
	# shellcheck disable=SC2059
	>&2 printf "$@"
	>&2 printf '\n'
	exit 1
}

print_usage()
{
    cat <<EOF
Re-configures and re-builds SOF with Zephyr using a pre-installed Zephyr toolchain and
platform's _defconfig file.

usage: $0 [options] platform(s)

       -a Build all platforms.
       -c Clone the complete Zephyr and SOF trees before building.
       -j n Set number of make build jobs. Jobs=#cores by default.
           Infinite when not specified.
       -k Path to a non-default rimage signing key.
       -p Zephyr root directory.

Supported platforms ${SUPPORTED_PLATFORMS[*]}

EOF
}

clone()
{
	# Check out Zephyr + SOF

	type -p west || die "Install west and a west toolchain following https://docs.zephyrproject.org/latest/getting_started/index.html"
	type -p git || die "Install git"

	[ -e "$ZEPHYR_ROOT" ] && die "$ZEPHYR_ROOT already exists"
	mkdir -p "$ZEPHYR_ROOT"
	cd "$ZEPHYR_ROOT"
	west init
	west update
	cd modules/audio/sof
	git remote add sof https://github.com/thesofproject/sof.git
	git remote update sof
	git checkout sof/main
	git clone --recurse-submodules https://github.com/thesofproject/rimage.git
	cd -
}

build()
{
	[ -d "$ZEPHYR_ROOT" ] || die "$ZEPHYR_ROOT doesn't exists"
	cd "$ZEPHYR_ROOT"

	# Build rimage
	RIMAGE_DIR=build-rimage
	mkdir -p "$RIMAGE_DIR"
	cd "$RIMAGE_DIR"
	cmake ../modules/audio/sof/rimage
	make -j"$BUILD_JOBS"
	cd -

	for platform in "${PLATFORMS[@]}"; do
		case "$platform" in
			apl)
				PLAT_CONFIG='intel_adsp_cavs15'
				;;
			cnl)
				PLAT_CONFIG='intel_adsp_cavs18'
				;;
			icl)
				PLAT_CONFIG='intel_adsp_cavs20'
				;;
			tgl-h)
				PLAT_CONFIG='intel_adsp_cavs25'
				;;
			*)
				echo "Unsupported platform: $platform"
				exit 1
				;;
		esac

		(
			set -x
			west build -d build-"$platform" -p -b "$PLAT_CONFIG" \
				zephyr/samples/subsys/audio/sof
			west sign -d build-"$platform" -t rimage -p "$RIMAGE_DIR"/rimage \
				-D modules/audio/sof/rimage/config -- -k "$RIMAGE_KEY"
		)
	done
}

main()
{
	# parse the args
	while getopts "acj:k:p:" OPTION; do
		case "$OPTION" in
			a) PLATFORMS=("${SUPPORTED_PLATFORMS[@]}") ;;
			c) DO_CLONE=yes ;;
			j) BUILD_JOBS="$OPTARG" ;;
			k) RIMAGE_KEY="$OPTARG" ;;
			p) ZEPHYR_ROOT="$OPTARG" ;;
			*) print_usage; exit 1 ;;
		esac
	done
	shift $((OPTIND-1))

	# parse platform args
	for arg in "$@"; do
		platform=none
		for i in "${SUPPORTED_PLATFORMS[@]}"; do
			if [ x"$i" = x"$arg" ]; then
				PLATFORMS=("${PLATFORMS[@]}" "$i")
				platform="$i"
				shift || true
				break
			fi
		done
		if [ "$platform" == "none" ]; then
			echo "Error: Unknown platform specified: $arg"
			echo "Supported platforms are: ${SUPPORTED_PLATFORMS[*]}"
			exit 1
		fi
	done

	# check target platform(s) have been passed in
	if [ "${#PLATFORMS[@]}" -eq 0 ]; then
		echo "Error: No platforms specified. Supported are: " \
		     "${SUPPORTED_PLATFORMS[*]}"
		print_usage
		exit 1
	fi

	if [ "x$DO_CLONE" == "xyes" ]; then
		clone
	fi

	build
}

main "$@"
