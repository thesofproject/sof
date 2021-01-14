#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# stop on most errors
set -e

SUPPORTED_PLATFORMS=(byt cht bdw hsw apl skl kbl cnl sue icl jsl \
                    imx8 imx8x imx8m tgl tgl-h)
BUILD_ROM=no
BUILD_DEBUG=no
BUILD_FORCE_UP=no
BUILD_JOBS=$(nproc --all)
BUILD_VERBOSE=
PLATFORMS=()

PATH=$pwd/local/bin:$PATH

pwd=$(pwd)

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
Re-configures and re-builds SOF using the corresponding compiler and
platform's _defconfig file.

usage: $0 [options] platform(s)

       -r Build rom if available (gcc only)
       -a Build all platforms
       -u Force UP ARCH
       -d Enable debug build
       -c Interactive menuconfig
       -o copies the file argument from src/arch/xtensa/configs/override/$arg.config
	  to the build directory after invoking CMake and before Make.
       -k Configure rimage to use a non-default \${RIMAGE_PRIVATE_KEY}
           DEPRECATED: use the more flexible \${PRIVATE_KEY_OPTION} below.
       -v Verbose Makefile log
       -j n Set number of make build jobs. Jobs=#cores when no flag. \
Infinte when not specified.
	-m path to MEU tool. Switches signing step to use MEU instead of rimage.
           To use a non-default key define PRIVATE_KEY_OPTION, see below.

To use a non-default key you must define the right CMake parameter in the
following environment variable:

     PRIVATE_KEY_OPTION='-DMEU_PRIVATE_KEY=path/to/key'  $0  -m /path/to/meu ...
or:
     PRIVATE_KEY_OPTION='-DRIMAGE_PRIVATE_KEY=path/to/key'  $0 ...

Supported platforms ${SUPPORTED_PLATFORMS[*]}

EOF
}

# parse the args
while getopts "rudj:ckvao:m:" OPTION; do
        case "$OPTION" in
		r) BUILD_ROM=yes ;;
		u) BUILD_FORCE_UP=yes ;;
		d) BUILD_DEBUG=yes ;;
		j) BUILD_JOBS=$OPTARG ;;
		c) MAKE_MENUCONFIG=yes ;;
		k) USE_PRIVATE_KEY=yes ;;
		o) OVERRIDE_CONFIG=$OPTARG ;;
		v) BUILD_VERBOSE='VERBOSE=1' ;;
		a) PLATFORMS=("${SUPPORTED_PLATFORMS[@]}") ;;
		m) MEU_TOOL_PATH=$OPTARG ;;
		*) print_usage; exit 1 ;;
        esac
done
shift $((OPTIND-1))

#default signing tool
SIGNING_TOOL=RIMAGE

if [ -n "${OVERRIDE_CONFIG}" ]
then
	OVERRIDE_CONFIG="src/arch/xtensa/configs/override/$OVERRIDE_CONFIG.config"
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
		echo "Supported platforms are: ${SUPPORTED_PLATFORMS[*]}"
		exit 1
	fi
done

# check target platform(s) have been passed in
if [ ${#PLATFORMS[@]} -eq 0 ];
then
	echo "Error: No platforms specified. Supported are: " \
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
WORKDIR="$pwd"

# build platforms
for platform in "${PLATFORMS[@]}"
do
	HAVE_ROM='no'
	DEFCONFIG_PATCH=''

	case $platform in
		byt)
			PLATFORM="baytrail"
			ARCH="xtensa"
			XTENSA_CORE="Intel_HiFiEP"
			HOST="xtensa-byt-elf"
			XTENSA_TOOLS_VERSION="RD-2012.5-linux"
			;;
		cht)
			PLATFORM="cherrytrail"
			ARCH="xtensa"
			XTENSA_CORE="CHT_audio_hifiep"
			HOST="xtensa-byt-elf"
			XTENSA_TOOLS_VERSION="RD-2012.5-linux"
			;;
		bdw)
			PLATFORM="broadwell"
			ARCH="xtensa"
			XTENSA_CORE="LX4_langwell_audio_17_8"
			HOST="xtensa-hsw-elf"
			XTENSA_TOOLS_VERSION="RG-2017.8-linux"
			;;
		hsw)
			PLATFORM="haswell"
			ARCH="xtensa"
			XTENSA_CORE="LX4_langwell_audio_17_8"
			HOST="xtensa-hsw-elf"
			XTENSA_TOOLS_VERSION="RG-2017.8-linux"
			;;
		apl)
			PLATFORM="apollolake"
			ARCH="xtensa-smp"
			XTENSA_CORE="X4H3I16w2D48w3a_2017_8"

			# test APL compiler aliases and ignore set -e here
			if type xtensa-bxt-elf-gcc; then
				HOST="xtensa-bxt-elf"
			else
				HOST="xtensa-apl-elf"
			fi

			XTENSA_TOOLS_VERSION="RG-2017.8-linux"
			HAVE_ROM='yes'
			;;
		skl)
			PLATFORM="skylake"
			ARCH="xtensa"
			XTENSA_CORE="X4H3I16w2D48w3a_2017_8"

			# test APL compiler aliases and ignore set -e here
			if type xtensa-bxt-elf-gcc; then
				HOST="xtensa-bxt-elf"
			else
				HOST="xtensa-apl-elf"
			fi

			XTENSA_TOOLS_VERSION="RG-2017.8-linux"
			HAVE_ROM='yes'
			;;
		kbl)
			PLATFORM="kabylake"
			ARCH="xtensa"
			XTENSA_CORE="X4H3I16w2D48w3a_2017_8"

			# test APL compiler aliases and ignore set -e here
			if type xtensa-bxt-elf-gcc; then
				HOST="xtensa-bxt-elf"
			else
				HOST="xtensa-apl-elf"
			fi

			XTENSA_TOOLS_VERSION="RG-2017.8-linux"
			HAVE_ROM='yes'
			;;
		cnl)
			PLATFORM="cannonlake"
			ARCH="xtensa-smp"
			XTENSA_CORE="X6H3CNL_2017_8"
			HOST="xtensa-cnl-elf"
			XTENSA_TOOLS_VERSION="RG-2017.8-linux"
			HAVE_ROM='yes'
			;;
		sue)
			PLATFORM="suecreek"
			ARCH="xtensa"
			XTENSA_CORE="X6H3CNL_2017_8"
			HOST="xtensa-cnl-elf"
			XTENSA_TOOLS_VERSION="RG-2017.8-linux"
			HAVE_ROM='yes'
			;;
		icl)
			PLATFORM="icelake"
			ARCH="xtensa-smp"
			XTENSA_CORE="X6H3CNL_2017_8"
			HOST="xtensa-cnl-elf"
			XTENSA_TOOLS_VERSION="RG-2017.8-linux"
			HAVE_ROM='yes'
			;;
		tgl)
			PLATFORM="tgplp"
			ARCH="xtensa-smp"
			XTENSA_CORE="cavs2x_LX6HiFi3_2017_8"
			HOST="xtensa-cnl-elf"
			XTENSA_TOOLS_VERSION="RG-2017.8-linux"
			HAVE_ROM='yes'
			# default key for TGL
			if [ -z "$PRIVATE_KEY_OPTION" ]
			then
				PRIVATE_KEY_OPTION="-D${SIGNING_TOOL}_PRIVATE_KEY=$pwd/keys/otc_private_key_3k.pem"
			fi
			;;
		tgl-h)
			PLATFORM="tgph"
			ARCH="xtensa-smp"
			XTENSA_CORE="cavs2x_LX6HiFi3_2017_8"
			HOST="xtensa-cnl-elf"
			XTENSA_TOOLS_VERSION="RG-2017.8-linux"
			HAVE_ROM='yes'
			# default key for TGL
			if [ -z "$PRIVATE_KEY_OPTION" ]
			then
				PRIVATE_KEY_OPTION="-D${SIGNING_TOOL}_PRIVATE_KEY=$pwd/keys/otc_private_key_3k.pem"
			fi
			;;
		jsl)
			PLATFORM="jasperlake"
			ARCH="xtensa-smp"
			XTENSA_CORE="X6H3CNL_2017_8"
			HOST="xtensa-cnl-elf"
			XTENSA_TOOLS_VERSION="RG-2017.8-linux"
			HAVE_ROM='yes'
			;;
		imx8)
			PLATFORM="imx8"
			ARCH="xtensa"
			XTENSA_CORE="hifi4_nxp_v3_3_1_2_dev"
			HOST="xtensa-imx-elf"
			XTENSA_TOOLS_VERSION="RF-2016.4-linux"
			;;
		imx8x)
			PLATFORM="imx8x"
			ARCH="xtensa"
			XTENSA_CORE="hifi4_nxp_v3_3_1_2_dev"
			HOST="xtensa-imx-elf"
			XTENSA_TOOLS_VERSION="RF-2016.4-linux"
			;;
		imx8m)
			PLATFORM="imx8m"
			ARCH="xtensa"
			XTENSA_CORE="hifi4_mscale_v0_0_2_prod"
			HOST="xtensa-imx8m-elf"
			XTENSA_TOOLS_VERSION="RF-2016.4-linux"
			;;

	esac
	ROOT="$pwd/../xtensa-root/$HOST"

	if [ -n "$XTENSA_TOOLS_ROOT" ]
	then
		XTENSA_TOOLS_DIR="$XTENSA_TOOLS_ROOT/install/tools/$XTENSA_TOOLS_VERSION"
		XTENSA_BUILDS_DIR="$XTENSA_TOOLS_ROOT/install/builds/$XTENSA_TOOLS_VERSION"

		# make sure the required version of xtensa tools is installed
		if [ -d "$XTENSA_TOOLS_DIR" ]
			then
				XCC="xt-xcc"
				XTOBJCOPY="xt-objcopy"
				XTOBJDUMP="xt-objdump"
			else
				XCC="none"
				XTOBJCOPY="none"
				XTOBJDUMP="none"
				echo "XTENSA_TOOLS_DIR is not a directory"
		fi
	fi

	# update ROOT directory for xt-xcc
	if [ "$XCC" == "xt-xcc" ]
	then
		TOOLCHAIN=xt
		ROOT="$XTENSA_BUILDS_DIR/$XTENSA_CORE/xtensa-elf"
		export XTENSA_SYSTEM=$XTENSA_BUILDS_DIR/$XTENSA_CORE/config
		PATH=$XTENSA_TOOLS_DIR/XtensaTools/bin:$OLDPATH
		COMPILER="xcc"
	else
		TOOLCHAIN=$HOST
		PATH=$pwd/../$HOST/bin:$OLDPATH
		COMPILER="gcc"

		case "$platform" in
			byt|cht|cnl|sue) DEFCONFIG_PATCH="_gcc";;
			*)	     DEFCONFIG_PATCH="";;
		esac
	fi

	BUILD_DIR=build_${platform}_${COMPILER}
	printf "Build in %s\n" "$BUILD_DIR"

	# only delete binary related to this build
	rm -fr "$BUILD_DIR"
	mkdir "$BUILD_DIR"
	cd "$BUILD_DIR"

	( set -x # log the main commands and their parameters
	cmake -DTOOLCHAIN="$TOOLCHAIN" \
		-DROOT_DIR="$ROOT" \
		-DMEU_OPENSSL="${MEU_OPENSSL}" \
		"${MEU_PATH_OPTION}" \
		"${PRIVATE_KEY_OPTION}" \
		..

	cmake --build .  --  ${PLATFORM}${DEFCONFIG_PATCH}_defconfig
	)

	if [ -n "$OVERRIDE_CONFIG" ]
	then
		cp "../$OVERRIDE_CONFIG" override.config
	fi

	if [[ "x$MAKE_MENUCONFIG" == "xyes" ]]
	then
		cmake --build .  --  menuconfig
	fi

	if [[ "x$BUILD_DEBUG" == "xyes" ]]
	then
		echo "CONFIG_DEBUG=y" >> override.config
	fi

	if [[ "x$BUILD_ROM" == "xyes" && "x$HAVE_ROM" == "xyes" ]]
	then
		echo "CONFIG_BUILD_VM_ROM=y" >> override.config
	fi

	# override default ARCH if BUILD_FORCE_UP is set
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

	cd "$WORKDIR"
done # for platform in ...

# list all the images
ls -l build_*/*.ri build_*/src/arch/xtensa/rom*.bin || true
ls -l build_*/sof
