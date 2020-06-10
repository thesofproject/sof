#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# stop on most errors
set -e

SUPPORTED_PLATFORMS=(byt cht bdw hsw apl skl kbl cnl sue icl jsl \
                    imx8 imx8x imx8m tgl)
BUILD_ROM=no
BUILD_DEBUG=no
BUILD_FORCE_UP=no
BUILD_JOBS=$(nproc --all)
BUILD_VERBOSE=
PLATFORMS=()

PATH=$pwd/local/bin:$PATH

pwd=$(pwd)

print_usage()
{
    cat <<EOF
Re-configures and re-builds SOF using the corresponding compiler and
platform's _defconfig file.

usage: xtensa-build.sh [options] platform(s)
       -r Build rom if available (gcc only)
       -a Build all platforms
       -u Force UP ARCH
       -d Enable debug build
       -c Interactive menuconfig
       -k Use private key
       -v Verbose Makefile log
       -j n Set number of make build jobs. Jobs=#cores when no flag. \
Infinte when not specified.
       Supported platforms ${SUPPORTED_PLATFORMS[*]}
EOF
}

# parse the args
while getopts "rudj:ckva" OPTION; do
        case "$OPTION" in
		r) BUILD_ROM=yes ;;
		u) BUILD_FORCE_UP=yes ;;
		d) BUILD_DEBUG=yes ;;
		j) BUILD_JOBS=$OPTARG ;;
		c) MAKE_MENUCONFIG=yes ;;
		k) USE_PRIVATE_KEY=yes ;;
		v) BUILD_VERBOSE='VERBOSE=1' ;;
		a) PLATFORMS=("${SUPPORTED_PLATFORMS[@]}") ;;
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
			platform=$i
			shift || true
			break
		fi
	done
	if [ $platform == "none" ]; then
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
	exit 1
fi

if [ "x$USE_PRIVATE_KEY" == "xyes" ]
then
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
			PLATFORM="tigerlake"
			ARCH="xtensa-smp"
			XTENSA_CORE="X6H3CNL_2017_8"
			HOST="xtensa-cnl-elf"
			XTENSA_TOOLS_VERSION="RG-2017.8-linux"
			HAVE_ROM='yes'
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

	if [ $XTENSA_TOOLS_ROOT ]
	then
		XTENSA_TOOLS_DIR="$XTENSA_TOOLS_ROOT/install/tools/$XTENSA_TOOLS_VERSION"
		XTENSA_BUILDS_DIR="$XTENSA_TOOLS_ROOT/install/builds/$XTENSA_TOOLS_VERSION"

		# make sure the required version of xtensa tools is installed
		if [ -d $XTENSA_TOOLS_DIR ]
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

		case $j in
			byt|cht|cnl|sue) DEFCONFIG_PATCH="_gcc";;
			*)	     DEFCONFIG_PATCH="";;
		esac
	fi

	BUILD_DIR=build_${platform}_${COMPILER}
	echo "Build in "$BUILD_DIR

	# only delete binary related to this build
	rm -fr $BUILD_DIR
	mkdir $BUILD_DIR
	cd $BUILD_DIR

	cmake -DTOOLCHAIN=$TOOLCHAIN \
		-DROOT_DIR=$ROOT \
		${PRIVATE_KEY_OPTION} \
		..

	make ${PLATFORM}${DEFCONFIG_PATCH}_defconfig

	if [[ "x$MAKE_MENUCONFIG" == "xyes" ]]
	then
		make menuconfig
	fi

	rm -fr override.config

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
		make overrideconfig
	fi

	if [ 'tgl' != "${platform}" ]; then
		make bin -j "${BUILD_JOBS}" ${BUILD_VERBOSE}
	else # FIXME: finish gcc support for tgl
		make sof -j "${BUILD_JOBS}" ${BUILD_VERBOSE}
		if [ "$BUILD_ROM" = "yes" ]; then
			make rom_dump  ${BUILD_VERBOSE}
		fi
	fi

	cd "$WORKDIR"
done # for platform in ...

# list all the images
ls -l build_*/*.ri build_*/src/arch/xtensa/rom*.bin || true
ls -l build_*/sof
