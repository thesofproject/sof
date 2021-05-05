#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation. All rights reserved.

# stop on most errors
set -e

SOF_TOP=$(cd "$(dirname "$0")" && cd .. && pwd)

SUPPORTED_PLATFORMS=(apl cnl icl tgl-h)
# Default value, can be overridden on the command line
WEST_TOP="${SOF_TOP}"/zephyrproject
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
the _defconfig file for that platform.

usage: $0 [options] platform(s)

       -a Build all platforms.
       -j n Set number of make build jobs. Jobs=#cores by default.
           Infinite when not specified.
       -k Path to a non-default rimage signing key.
       -c recursively clones Zephyr inside sof before building.
          Incompatible with -p.
       -p Existing Zephyr project directory. Incompatible with -c.
          If modules/audio/sof is missing there then a symbolic
          link pointing to ${SOF_TOP} will be added.

Supported platforms ${SUPPORTED_PLATFORMS[*]}

EOF
}

# Downloads zephyrproject inside sof/ and create a ../../.. symbolic
# link back to sof/
clone()
{
	type -p west || die "Install west and a west toolchain following https://docs.zephyrproject.org/latest/getting_started/index.html"
	type -p git || die "Install git"

	[ -e "$WEST_TOP" ] && die "$WEST_TOP already exists"
	mkdir -p "$WEST_TOP"
	git clone --depth=5 https://github.com/zephyrproject-rtos/zephyr \
	    "$WEST_TOP"/zephyr
	git -C "$WEST_TOP"/zephyr --no-pager log --oneline --graph \
	    --decorate --max-count=20
	west init -l "${WEST_TOP}"/zephyr
	( cd "${WEST_TOP}"
	  mkdir -p modules/audio
	  ln -s ../../.. modules/audio/sof
	  # Do NOT "west update sof"!!
	  west update zephyr hal_xtensa
	)
}

build()
{
	cd "$WEST_TOP"

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
	local zeproj

	# parse the args
	while getopts "acj:k:p:" OPTION; do
		case "$OPTION" in
			a) PLATFORMS=("${SUPPORTED_PLATFORMS[@]}") ;;
			c) DO_CLONE=yes ;;
			j) BUILD_JOBS="$OPTARG" ;;
			k) RIMAGE_KEY="$OPTARG" ;;
			p) zeproj="$OPTARG" ;;
			*) print_usage; exit 1 ;;
		esac
	done
	shift $((OPTIND-1))

	if [ -n "$zeproj" ] && [ x"$DO_CLONE" = xyes ]; then
	    die 'Cannot use -p with -c, -c supports %s only' "${WEST_TOP}"
	fi

	if [ -n "$zeproj" ]; then

	    [ -d "$zeproj" ] ||
		die "$zeproj is not a directory, try -c instead of -p?"

	    ( cd "$zeproj"
	      test "$(realpath "$(west topdir)")" = "$(/bin/pwd)"
	    ) || die '%s is not a zephyrproject' "$WEST_TOP"

	    WEST_TOP="$zeproj"
	fi

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
	else
		# Link to ourselves if no sof module yet
		test -e "${WEST_TOP}"/modules/audio/sof || {
		    mkdir -p "${WEST_TOP}"/modules/audio
		    ln -s "$SOF_TOP" "${WEST_TOP}"/modules/audio/sof
		}

		# Support for submodules in west is too recent, cannot
		# rely on it
		test -e "${WEST_TOP}"/modules/audio/sof/rimage/CMakeLists.txt || (
		    cd "${WEST_TOP}"/modules/audio/sof
		    git submodule update --init --recursive
		)
	fi

	build
}

main "$@"
