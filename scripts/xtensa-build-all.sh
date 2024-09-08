#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# stop on most errors
set -e

# Platforms built and tested by default in CI using the `-a` option.
# They must have a toolchain available in the latest Docker image.
DEFAULT_PLATFORMS=(
    imx8m
    rn rmb vangogh
    mt8186 mt8195 mt8188
)

# Work in progress can be added to this "staging area" without breaking
# the -a option for everyone.
SUPPORTED_PLATFORMS=( "${DEFAULT_PLATFORMS[@]}" )

# Container work is in progress
SUPPORTED_PLATFORMS+=( acp_6_3 acp_7_0 )

BUILD_ROM=no
BUILD_DEBUG=no
BUILD_FORCE_UP=no
BUILD_JOBS=$(nproc --all)
BUILD_VERBOSE=
PLATFORMS=()

SOF_TOP=$(cd "$(dirname "$0")/.." && pwd)

# As CMake forks one compiler process for each source file, the XTensa
# compiler spends much more time idle waiting for the license server
# over the network than actually using CPU or disk. A factor 3 has been
# found optimal for 16 nproc 25ms away from the server; your mileage may
# vary.
#
# The entire, purely local gcc build is so fast (~ 1s) that observing
# any difference between -j nproc and -j nproc*N is practically
# impossible so let's not waste RAM when building with gcc.

if [ -n "$XTENSA_TOOLS_ROOT" ]; then
    BUILD_JOBS=$((BUILD_JOBS * 3))
fi


die()
{
	>&2 printf '%s ERROR: ' "$0"
	# We want die() to be usable exactly like printf
	# shellcheck disable=SC2059
	>&2 printf "$@"
	exit 1
}

print_usage()
{
    cat <<EOF
Re-configures and re-builds SOF using the corresponding compiler and the
<platform>_defconfig file. Implements and saves the manual configuration
described in
https://thesofproject.github.io/latest/developer_guides/firmware/cmake.html

usage: $0 [options] platform(s)

       -r Build rom if available (gcc only)
       -a Build all default platforms fully supported by the latest Docker image and CI
       -u Force CONFIG_MULTICORE=n
       -d Enable debug build
       -c Interactive menuconfig
       -o arg, copies src/arch/xtensa/configs/override/<arg>.config
          to the build directory after invoking CMake and before Make.
       -k Configure rimage to use a non-default \${RIMAGE_PRIVATE_KEY}
          DEPRECATED: use the more flexible \${PRIVATE_KEY_OPTION} below.
       -v Verbose Makefile log
       -i Optional IPC_VERSION: can be set to IPC3, IPC4 or an empty string.
          If set to "IPCx" then CONFIG_IPC_MAJOR_x will be set. If set to
          IPC4 then a platform specific overlay may be used.
       -j n Set number of make build jobs. Jobs=#cores when no flag.
            Infinite when not specified.
       -m path to MEU tool. CMake disables rimage signing which produces a
          .uns[igned] file signed by MEU. For a non-default key use the
          PRIVATE_KEY_OPTION, see below.

To use a non-default key you must define the right CMake parameter in the
following environment variable:

     PRIVATE_KEY_OPTION='-DMEU_PRIVATE_KEY=path/to/key'  $0  -m /path/to/meu ...
or:
     PRIVATE_KEY_OPTION='-DRIMAGE_PRIVATE_KEY=path/to/key'  $0 ...

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

Known platforms: ${SUPPORTED_PLATFORMS[*]}

EOF
}

# parse the args
while getopts "rudi:j:ckvao:m:" OPTION; do
        case "$OPTION" in
		r) BUILD_ROM=yes ;;
		u) BUILD_FORCE_UP=yes ;;
		d) BUILD_DEBUG=yes ;;
		i) IPC_VERSION=$OPTARG ;;
		j) BUILD_JOBS=$OPTARG ;;
		c) MAKE_MENUCONFIG=yes ;;
		k) USE_PRIVATE_KEY=yes ;;
		o) OVERRIDE_CONFIG=$OPTARG ;;
		v) BUILD_VERBOSE='VERBOSE=1' ;;
		a) PLATFORMS=("${DEFAULT_PLATFORMS[@]}") ;;
		m) MEU_TOOL_PATH=$OPTARG ;;
		*) print_usage; exit 1 ;;
        esac
done
shift $((OPTIND-1))

#default signing tool
SIGNING_TOOL=RIMAGE

if [ -n "${OVERRIDE_CONFIG}" ]
then
	OVERRIDE_CONFIG="${SOF_TOP}/src/arch/xtensa/configs/override/$OVERRIDE_CONFIG.config"
	[ -f "${OVERRIDE_CONFIG}" ] || die 'Invalid override config file %s\n' "${OVERRIDE_CONFIG}"
fi

if [ -n "${MEU_TOOL_PATH}" ]
then
	[ -d "${MEU_TOOL_PATH}" ] || die 'Invalid MEU TOOL PATH %s\n' "${MEU_TOOL_PATH}"
	MEU_PATH_OPTION=-DMEU_PATH="${MEU_TOOL_PATH}"
	SIGNING_TOOL=MEU
fi

# parse platform args
for arg in "$@"; do
	platform=none
	for i in "${SUPPORTED_PLATFORMS[@]}"; do
		if [ x"$i" = x"$arg" ]; then
			PLATFORMS=("${PLATFORMS[@]}" "$i")
			platform=$i
			shift || true
			break
		fi
	done
	if [ "$platform" == "none" ]; then
		echo "Error: Unknown platform specified: $arg"
		echo "Known platforms are: ${SUPPORTED_PLATFORMS[*]}"
		exit 1
	fi
done

# check target platform(s) have been passed in
if [ ${#PLATFORMS[@]} -eq 0 ];
then
	echo "Error: No platform specified. Known platforms: " \
		"${SUPPORTED_PLATFORMS[*]}"
	print_usage
	exit 1
fi

if [ "x$USE_PRIVATE_KEY" == "xyes" ]
then
	>&2 printf \
	    'WARNING: -k and RIMAGE_PRIVATE_KEY are deprecated, see usage.\n'
	if [ -z ${RIMAGE_PRIVATE_KEY+x} ]
	then
		echo "Error: No variable specified for RIMAGE_PRIVATE_KEY"
		exit 1
	fi
	PRIVATE_KEY_OPTION="-DRIMAGE_PRIVATE_KEY=${RIMAGE_PRIVATE_KEY}"
fi

OLDPATH=$PATH
CURDIR="$(pwd)"

# build platforms
for platform in "${PLATFORMS[@]}"
do

	printf '\n   ------\n   %s\n   ------\n' "$platform"

	HAVE_ROM='no'
	DEFCONFIG_PATCH=''
	PLATFORM_PRIVATE_KEY=''

	source "${SOF_TOP}"/scripts/set_xtensa_params.sh "$platform" ||
            die 'set_xtensa_params.sh failed'

	test -z "${PRIVATE_KEY_OPTION}" || PLATFORM_PRIVATE_KEY="${PRIVATE_KEY_OPTION}"

	if [ -n "$XTENSA_TOOLS_ROOT" ]
	then
		XTENSA_TOOLS_DIR="$XTENSA_TOOLS_ROOT/install/tools/$TOOLCHAIN_VER"
		XTENSA_BUILDS_DIR="$XTENSA_TOOLS_ROOT/install/builds/$TOOLCHAIN_VER"

		[ -d "$XTENSA_TOOLS_DIR" ] || {
			>&2 printf 'ERROR: %s\t is not a directory\n' "$XTENSA_TOOLS_DIR"
			exit 1
		}
	fi

	# CMake uses ROOT_DIR for includes and libraries a bit like
	# --sysroot would.
	ROOT="$SOF_TOP/../xtensa-root/$HOST"

	if [ -n "$XTENSA_TOOLS_ROOT" ]
	then
		TOOLCHAIN=xt
		ROOT="$XTENSA_BUILDS_DIR/$XTENSA_CORE/xtensa-elf"
		# CMake cannot set (evil) build-time environment variables at configure time:
# https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#how-can-i-get-or-set-environment-variables
		export XTENSA_SYSTEM=$XTENSA_BUILDS_DIR/$XTENSA_CORE/config
		printf 'XTENSA_SYSTEM=%s\n' "${XTENSA_SYSTEM}"
		PATH=$XTENSA_TOOLS_DIR/XtensaTools/bin:$OLDPATH
		build_dir_suffix='xcc'
	else
		# Override SOF_CC_BASE from set_xtensa_params.sh
		SOF_CC_BASE='gcc'
		TOOLCHAIN=$HOST
		PATH=$SOF_TOP/../$HOST/bin:$OLDPATH
		build_dir_suffix='gcc'
		DEFCONFIG_PATCH=""
	fi

	BUILD_DIR=build_${platform}_${build_dir_suffix}
	printf "Build in %s\n" "$BUILD_DIR"

	# only delete binary related to this build
	rm -fr "$BUILD_DIR"
	mkdir "$BUILD_DIR"
	cd "$BUILD_DIR"

	printf 'PATH=%s\n' "$PATH"
	( set -x # log the main commands and their parameters
	cmake -DTOOLCHAIN="$TOOLCHAIN" \
		-DSOF_CC_BASE="$SOF_CC_BASE" \
		-DROOT_DIR="$ROOT" \
		-DMEU_OPENSSL="${MEU_OPENSSL}" \
		"${MEU_PATH_OPTION}" \
		"${PLATFORM_PRIVATE_KEY}" \
		-DINIT_CONFIG=${PLATFORM}${DEFCONFIG_PATCH}_defconfig \
		-DEXTRA_CFLAGS="${EXTRA_CFLAGS}" \
		"$SOF_TOP"
	)

	if [ -n "$OVERRIDE_CONFIG" ]
	then
		cp "$OVERRIDE_CONFIG" override.config
	fi

	if [[ "x$MAKE_MENUCONFIG" == "xyes" ]]
	then
		cmake --build .  --  menuconfig
	fi

	case "$IPC_VERSION" in
	    '') ;;
	    IPC3)
		echo 'CONFIG_IPC_MAJOR_3=y' >> override.config
		;;
	    IPC4)
		test -z "$IPC4_CONFIG_OVERLAY" ||
	    cat "${SOF_TOP}/src/arch/xtensa/configs/override/$IPC4_CONFIG_OVERLAY.config" \
		    >> override.config
		echo 'CONFIG_IPC_MAJOR_4=y' >> override.config
	    ;;
	    *) die "Invalid -i '%s' argument\n" "$IPC_VERSION" ;;
	esac

	if [[ "x$BUILD_DEBUG" == "xyes" ]]
	then
		echo "CONFIG_DEBUG=y" >> override.config
	fi

	if [[ "x$BUILD_ROM" == "xyes" && "x$HAVE_ROM" == "xyes" ]]
	then
		echo "CONFIG_BUILD_VM_ROM=y" >> override.config
	fi

	if [ "x$BUILD_FORCE_UP" == "xyes" ]
	then
		echo "Force building UP(xtensa)..."
		echo "CONFIG_MULTICORE=n" >> override.config
	fi

	if [ -e override.config ]
	then
		cmake --build .  --  overrideconfig
	fi

	cmake --build .  --  bin -j "${BUILD_JOBS}" ${BUILD_VERBOSE}

	cd "$CURDIR"
done # for platform in ...

# list all the images
ls -l build_*/*.ri build_*/src/arch/xtensa/rom*.bin || true
ls -l build_*/sof
