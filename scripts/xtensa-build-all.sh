#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

SUPPORTED_PLATFORMS=(byt cht bdw hsw apl cnl sue icl skl kbl imx8)
BUILD_ROM=no
BUILD_DEBUG=no
BUILD_FORCE_UP=no
BUILD_JOBS=$(nproc --all)
BUILD_JOBS_NEXT=0

PATH=$pwd/local/bin:$PATH

pwd=`pwd`

if [ "$#" -eq 0 ]
then
	echo "usage: xtensa-build.sh [options] platform(s)"
	echo "       [-r] Build rom (gcc only)"
	echo "       [-a] Build all platforms"
	echo "       [-u] Force UP ARCH"
	echo "       [-d] Enable debug build"
	echo "       [-c] Configure defconfig"
	echo "       [-k] Use private key"
	echo "       [-j [n]] Set number of make build jobs." \
		"Jobs=#cores when no flag. Inifinte when no arg."
	echo "       Supported platforms ${SUPPORTED_PLATFORMS[@]}"
else
	# parse the args
	for args in $@
	do
		if [[ "$args" == "-r" ]]
			then
			BUILD_ROM=yes

		elif [[ "$args" == "-u" ]]
			then
			BUILD_FORCE_UP=yes

		elif [[ "$args" == "-d" ]]
			then
			BUILD_DEBUG=yes

		elif [[ "$args" == "-j" ]]
			then
			BUILD_JOBS_NEXT=1
			BUILD_JOBS=""

		elif [[ "$args" == "-c" ]]
			then
			MAKE_MENUCONFIG=yes

		elif [[ "$args" == "-k" ]]
			then
			USE_PRIVATE_KEY=yes

		# Build all platforms
		elif [[ "$args" == "-a" ]]
			then
			PLATFORMS=${SUPPORTED_PLATFORMS[@]}
		else
			# check for plaform
			for i in ${SUPPORTED_PLATFORMS[@]}
			do
				if [ $i == $args ]
				then
					PLATFORMS+=$i" "
					BUILD_JOBS_NEXT=0
				fi
			done

			# check for jobs
			if [ ${BUILD_JOBS_NEXT} == 1 ]
				then
				BUILD_JOBS=$args
				BUILD_JOBS_NEXT=0
			fi
		fi
	done
fi

# check target platform(s) have been passed in
if [ ${#PLATFORMS[@]} -eq 0 ];
then
	echo "Error: No platforms specified. Supported are: ${SUPPORTED_PLATFORMS[@]}"
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

# fail on any errors
set -e

OLDPATH=$PATH
WORKDIR="$pwd"

# build platform
for j in ${PLATFORMS[@]}
do
	HAVE_ROM='no'
	if [ $j == "byt" ]
	then
		PLATFORM="baytrail"
		ARCH="xtensa"
		XTENSA_CORE="Intel_HiFiEP"
		ROOT="$pwd/../xtensa-root/xtensa-byt-elf"
		HOST="xtensa-byt-elf"
		XTENSA_TOOLS_VERSION="RD-2012.5-linux"
	fi
	if [ $j == "cht" ]
	then
		PLATFORM="cherrytrail"
		ARCH="xtensa"
		XTENSA_CORE="CHT_audio_hifiep"
		ROOT="$pwd/../xtensa-root/xtensa-byt-elf"
		HOST="xtensa-byt-elf"
		XTENSA_TOOLS_VERSION="RD-2012.5-linux"
	fi
	if [ $j == "bdw" ]
	then
		PLATFORM="broadwell"
		ARCH="xtensa"
		ROOT="$pwd/../xtensa-root/xtensa-hsw-elf"
		HOST="xtensa-hsw-elf"
	fi
	if [ $j == "hsw" ]
	then
		PLATFORM="haswell"
		ARCH="xtensa"
		ROOT="$pwd/../xtensa-root/xtensa-hsw-elf"
		HOST="xtensa-hsw-elf"
	fi
	if [ $j == "apl" ]
	then
		PLATFORM="apollolake"
		ARCH="xtensa-smp"
		XTENSA_CORE="X4H3I16w2D48w3a_2017_8"

		# test APL compiler aliases and ignore set -e here
		type xtensa-bxt-elf-gcc > /dev/null 2>&1 && true
		if [ $? == 0 ]
		then
			HOST="xtensa-bxt-elf"
		else
			HOST="xtensa-apl-elf"
		fi

		ROOT="$pwd/../xtensa-root/$HOST"
		XTENSA_TOOLS_VERSION="RG-2017.8-linux"
		HAVE_ROM='yes'
	fi
	if [ $j == "skl" ]
	then
		PLATFORM="skylake"
		ARCH="xtensa"
		XTENSA_CORE="X4H3I16w2D48w3a_2017_8"

		# test APL compiler aliases and ignore set -e here
		type xtensa-bxt-elf-gcc > /dev/null 2>&1 && true
		if [ $? == 0 ]
		then
			HOST="xtensa-bxt-elf"
		else
			HOST="xtensa-apl-elf"
		fi

		ROOT="$pwd/../xtensa-root/$HOST"
		XTENSA_TOOLS_VERSION="RG-2017.8-linux"
		HAVE_ROM='yes'
	fi
	if [ $j == "kbl" ]
	then
		PLATFORM="kabylake"
		ARCH="xtensa"
		XTENSA_CORE="X4H3I16w2D48w3a_2017_8"

		# test APL compiler aliases and ignore set -e here
		type xtensa-bxt-elf-gcc > /dev/null 2>&1 && true
		if [ $? == 0 ]
		then
			HOST="xtensa-bxt-elf"
		else
			HOST="xtensa-apl-elf"
		fi

		ROOT="$pwd/../xtensa-root/$HOST"
		XTENSA_TOOLS_VERSION="RG-2017.8-linux"
		HAVE_ROM='yes'
	fi
	if [ $j == "cnl" ]
	then
		PLATFORM="cannonlake"
		ARCH="xtensa-smp"
		XTENSA_CORE="X6H3CNL_2016_4_linux"
		ROOT="$pwd/../xtensa-root/xtensa-cnl-elf"
		HOST="xtensa-cnl-elf"
		XTENSA_TOOLS_VERSION="RF-2016.4-linux"
		HAVE_ROM='yes'
	fi
	if [ $j == "sue" ]
        then
                PLATFORM="suecreek"
		ARCH="xtensa"
                XTENSA_CORE="X6H3CNL_2016_4_linux"
                ROOT="$pwd/../xtensa-root/xtensa-cnl-elf"
                HOST="xtensa-cnl-elf"
                XTENSA_TOOLS_VERSION="RF-2016.4-linux"
		HAVE_ROM='yes'
        fi
	if [ $j == "icl" ]
	then
		PLATFORM="icelake"
		ARCH="xtensa-smp"
		XTENSA_CORE="X6H3CNL_2016_4_linux"
		ROOT="$pwd/../xtensa-root/xtensa-cnl-elf"
		HOST="xtensa-cnl-elf"
		XTENSA_TOOLS_VERSION="RF-2016.4-linux"
		HAVE_ROM='yes'
	fi
	if [ $j == "imx8" ]
	then
		PLATFORM="imx8"
		ARCH="xtensa"
		ROOT="$pwd/../xtensa-root/xtensa-imx-elf"
		HOST="xtensa-imx-elf"
		XTENSA_TOOLS_VERSION="RF-2016.4-linux"
                XTENSA_TOOLS_ROOT="/work/repos/imx-audio-toolchain/Xtensa_Tool"
	fi
	if [ $XTENSA_TOOLS_ROOT ]
	then
		XTENSA_TOOLS_DIR="$XTENSA_TOOLS_ROOT/tools/$XTENSA_TOOLS_VERSION"
		XTENSA_BUILDS_DIR="$XTENSA_TOOLS_ROOT/builds/$XTENSA_TOOLS_VERSION"

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
	fi

	BUILD_DIR=build_${j}_${COMPILER}
	echo "Build in "$BUILD_DIR

	# only delete binary related to this build
	rm -fr $BUILD_DIR
	mkdir $BUILD_DIR
	cd $BUILD_DIR

	cmake -DTOOLCHAIN=$TOOLCHAIN \
		-DROOT_DIR=$ROOT \
		-DCMAKE_VERBOSE_MAKEFILE=ON \
		${PRIVATE_KEY_OPTION} \
		..

	make ${PLATFORM}_defconfig

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
		echo "CONFIG_SMP=n" >> override.config
	fi

	if [ -e override.config ]
	then
		make overrideconfig
	fi

	make bin -j ${BUILD_JOBS}

	cd "$WORKDIR"
done

# list all the images
ls -l build_*/*.ri
