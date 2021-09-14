#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation. All rights reserved.

# stop on most errors
set -e

SOF_TOP=$(cd "$(dirname "$0")" && cd .. && pwd)

SUPPORTED_PLATFORMS=()

# Intel
SUPPORTED_PLATFORMS+=(apl cnl icl tgl-h tgl)

# NXP
SUPPORTED_PLATFORMS+=(imx8)

# Default value, can (and sometimes must) be overridden with -p
WEST_TOP="${SOF_TOP}"/zephyrproject
BUILD_JOBS=$(nproc --all)
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

usage: $0 [options] [ platform(s) ] [ -- cmake arguments ]

       -a Build all platforms.
       -j n Set number of make build jobs for rimage. Jobs=#cores by default.
           Ignored by "west build".
       -k Path to a non-default rimage signing key.
       -c recursively clones Zephyr inside sof before building.
          Incompatible with -p. To stop after cloning Zephyr, do not
          pass any platform or cmake argument.
       -p Existing Zephyr project directory. Incompatible with -c.  If
          zephyr-project/modules/audio/sof is missing then a
          symbolic link pointing to ${SOF_TOP} will automatically be
          created and west will recognize it as a new sof module.
          This -p option is always _required_ if the real (not symbolic)
          sof/ and zephyr-project/ directories are not nested in one
          another.

This script supports XtensaTools but only when installed in a specific
directory structure, example:

myXtensa/
└── install/
    ├── builds/
    │   ├── RD-2012.5-linux/
    │   │   └── Intel_HiFiEP/
    │   └── RG-2017.8-linux/
    │       ├── LX4_langwell_audio_17_8/
    │       └── X4H3I16w2D48w3a_2017_8/
    └── tools/
        ├── RD-2012.5-linux/
        │   └── XtensaTools/
        └── RG-2017.8-linux/
            └── XtensaTools/

$ XTENSA_TOOLS_ROOT=/path/to/myXtensa $0 ...

Supported platforms ${SUPPORTED_PLATFORMS[*]}

EOF
}

zephyr_fetch_and_switch()
{
	( cd "$WEST_TOP"/zephyr
	  set -x
	  git fetch --depth=5 "$1" "$2"
	  git checkout FETCH_HEAD
	)
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

	# This shows how to point SOF CI to and run all SOF tests on any
	# Zephyr work in progress or any other Zephyr commit from
	# anywhere. Simply edit remote and reference, uncomment this
	# line and submit as an SOF Pull Request.

	zephyr_fetch_and_switch    origin   pull/38319/head

	# SECURITY WARNING for reviewers: never allow unknown code from
	# unknown submitters on any CI system.

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

install_opts()
{
    install -D -p "$@"
}

build_all()
{
	cd "$WEST_TOP"
	west topdir || {
	    printf 'SOF_TOP=%s\nWEST_TOP=%s\n' "$SOF_TOP" "$WEST_TOP"
	    die 'try the -p option?'
	    # Also note west can get confused by symbolic links, see
	    # https://github.com/zephyrproject-rtos/west/issues/419
	}

	local STAGING=build-sof-staging
	mkdir -p ${STAGING}/sof/ # smex does not use 'install -D'

	for platform in "${PLATFORMS[@]}"; do
		# TODO: extract a new build_platform() function for
		# cleaner error handling and saving a lot of tabs.

		# default value
		local RIMAGE_KEY=modules/audio/sof/keys/otc_private_key.pem

		case "$platform" in
			apl)
				PLAT_CONFIG='intel_adsp_cavs15'
				# XCC build runs out of memory, tracked as
				# https://github.com/thesofproject/sof/issues/4645
				unset XTENSA_TOOLS_ROOT
				#XTENSA_CORE="X4H3I16w2D48w3a_2017_8"
				#XTENSA_TOOLS_VERSION="RG-2017.8-linux"
				;;
			cnl)
				PLAT_CONFIG='intel_adsp_cavs18'
				XTENSA_CORE="X6H3CNL_2017_8"
				XTENSA_TOOLS_VERSION="RG-2017.8-linux"
				;;
			icl)
				PLAT_CONFIG='intel_adsp_cavs20'
				XTENSA_CORE="X6H3CNL_2017_8"
				XTENSA_TOOLS_VERSION="RG-2017.8-linux"
				;;
			tgl-h|tgl)
				PLAT_CONFIG='intel_adsp_cavs25'
				XTENSA_CORE="cavs2x_LX6HiFi3_2017_8"
				XTENSA_TOOLS_VERSION="RG-2017.8-linux"
				RIMAGE_KEY=modules/audio/sof/keys/otc_private_key_3k.pem
				;;
			imx8)
				PLAT_CONFIG='nxp_adsp_imx8'
				RIMAGE_KEY='' # no key needed for imx8
				;;
			*)
				echo "Unsupported platform: $platform"
				exit 1
				;;
		esac

		if [ -n "$XTENSA_TOOLS_ROOT" ]
		then
			# set variables expected by zephyr/cmake/toolchain/xcc/generic.cmake
			export ZEPHYR_TOOLCHAIN_VARIANT=xcc
			export XTENSA_TOOLCHAIN_PATH="$XTENSA_TOOLS_ROOT/install/tools"
			export TOOLCHAIN_VER="$XTENSA_TOOLS_VERSION"
			printf 'XTENSA_TOOLCHAIN_PATH=%s\n' "${XTENSA_TOOLCHAIN_PATH}"
			printf 'TOOLCHAIN_VER=%s\n' "${TOOLCHAIN_VER}"

			# set variables expected by xcc toolchain
			XTENSA_BUILDS_DIR="$XTENSA_TOOLS_ROOT/install/builds/$XTENSA_TOOLS_VERSION"
			export XTENSA_SYSTEM=$XTENSA_BUILDS_DIR/$XTENSA_CORE/config
			printf 'XTENSA_SYSTEM=%s\n' "${XTENSA_SYSTEM}"
		fi

		local bdir=build-"$platform"

		(
			 # west can get lost in symbolic links and then
			 # show a confusing error.
			/bin/pwd
			set -x
			west build --build-dir "$bdir" --board "$PLAT_CONFIG" \
				zephyr/samples/subsys/audio/sof \
				-- "${CMAKE_ARGS[@]}"

			# This should ideally be part of
			# sof/zephyr/CMakeLists.txt but due to the way
			# Zephyr works, the SOF library build there does
			# not seem like it can go further than a
			# "libmodules_sof.a" file that smex does not
			# know how to use.
			"$bdir"/zephyr/smex_ep/build/smex \
			       -l "$STAGING"/sof/sof-"$platform".ldc \
			       "$bdir"/zephyr/zephyr.elf

			# Build rimage
			RIMAGE_DIR=build-rimage
			cmake -B "$RIMAGE_DIR" -S modules/audio/sof/rimage
			make -C "$RIMAGE_DIR" -j"$BUILD_JOBS"

			test -z "${RIMAGE_KEY_OPT}" || RIMAGE_KEY="${RIMAGE_KEY_OPT}"

			west sign  --build-dir "$bdir" \
				--tool rimage --tool-path "$RIMAGE_DIR"/rimage \
				--tool-data modules/audio/sof/rimage/config -- -k "$RIMAGE_KEY"
		)
		install_opts -m 0644 "$bdir"/zephyr/zephyr.ri \
			     "$STAGING"/sof/community/sof-"$platform".ri
	done

	install_opts -m 0755 "$bdir"/zephyr/sof-logger_ep/build/logger/sof-logger \
		     "$STAGING"/tools/sof-logger

	tree "$STAGING" || ( cd "$STAGING" && ls -R1 )
}

parse_args()
{
	local zeproj

	local OPTIND=1

	# Parse -options
	while getopts "acj:k:p:" OPTION; do
		case "$OPTION" in
			a) PLATFORMS=("${SUPPORTED_PLATFORMS[@]}") ;;
			c) DO_CLONE=yes ;;
			j) BUILD_JOBS="$OPTARG" ;;
			k) RIMAGE_KEY_OPT="$OPTARG" ;;
			p) zeproj="$OPTARG" ;;
			*) print_usage; exit 1 ;;
		esac
	done
	# This also drops any _leading_ '--'. Note this parser is
	# compatible with using '--' twice (undocumented feature), as
	# in:
	#   build-zephyr.sh -c -k somekey -- apl imx -- -DEXTRA_CFLAGS='-Werror'
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

	# Find platform arguments if '-a' was not used
	test "${#PLATFORMS[@]}" -ne 0 || while test -n "$1"; do
		local arg="$1"

		# '--' ends platforms
		if [ x"$arg" = x-- ]; then
		    shift || true; break
		fi

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

	# Check some target platform(s) have been passed in one way or
	# the other
	if [ "${#PLATFORMS[@]}" -eq 0 ]; then
		printf 'No platform build requested\n'
	else
		printf 'Building platforms:'
		printf ' %s' "${PLATFORMS[@]}"; printf '\n'
	fi

	CMAKE_ARGS=("$@")
	# For debugging quoting and whitespace
	#declare -p CMAKE_ARGS
}

main()
{
	parse_args "$@"

	if [ "x$DO_CLONE" == "xyes" ]; then
		clone
	else
		# Symlink zephyr-project to our SOF selves if no sof west module yet
		test -e "${WEST_TOP}"/modules/audio/sof || {
		    mkdir -p "${WEST_TOP}"/modules/audio
		    ln -s "$SOF_TOP" "${WEST_TOP}"/modules/audio/sof
		}

		# FIXME: remove this hack. Downloading and building
		# should be kept separate but support for submodules in
		# west is too recent, cannot rely on it yet.
		test -e "${WEST_TOP}"/modules/audio/sof/rimage/CMakeLists.txt || (
		    cd "${WEST_TOP}"/modules/audio/sof
		    git submodule update --init --recursive
		)
	fi

	test "${#PLATFORMS[@]}" -eq 0 || build_all
}

main "$@"
