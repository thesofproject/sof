#!/bin/bash

SUPPORTED_PLATFORMS=(byt cht bdw hsw apl cnl sue icl skl kbl)
BUILD_RIMAGE=0
BUILD_DEBUG=no

pwd=`pwd`

if [ "$#" -eq 0 ]
then
	echo "usage: xtensa-build.sh [options] platform(s)"
	echo "       [-l] Build rimage locally"
	echo "       [-a] Build all platforms"
	echo "       [-d] Enable debug build"
	echo "       Supported platforms ${SUPPORTED_PLATFORMS[@]}"
else
	# parse the args
	for args in $@
	do
		if [[ "$args" == "-l" ]]
			then
			BUILD_LOCAL=1
			BUILD_RIMAGE=1

			PATH=$pwd/local/bin:$PATH

		elif [[ "$args" == "-d" ]]
			then
			BUILD_DEBUG=yes

		# Build all platforms
		elif [[ "$args" == "-a" ]]
			then
			PLATFORMS=${SUPPORTED_PLATFORMS[@]}
		else
			for i in ${SUPPORTED_PLATFORMS[@]}
			do
				if [ $i == $args ]
				then
					PLATFORMS+=$i" "
				fi
			done
		fi
	done
fi

# check target platform(s) have been passed in
if [ ${#PLATFORMS[@]} -eq 0 ];
then
	echo "Error: No platforms specified. Supported are: ${SUPPORTED_PLATFORMS[@]}"
	exit 1
fi

# fail on any errors
set -e

# run autogen.sh
./autogen.sh

# make sure rimage is built and aligned with code
if [[ "x$BUILD_RIMAGE" == "x1" ]]
then
	if [[ "x$BUILD_LOCAL" == "x" ]]
	then
		./configure --enable-rimage
		make
		sudo make install
	else
		echo "BUILD in local folder!"
		rm -rf $pwd/local/
		./configure --enable-rimage --prefix=$pwd/local
		make
		make install
		PATH=$pwd/local/bin:$PATH
	fi
fi

OLDPATH=$PATH

# build platform
for j in ${PLATFORMS[@]}
do
	if [ $j == "byt" ]
	then
		PLATFORM="baytrail"
		XTENSA_CORE="Intel_HiFiEP"
		ROOT="$pwd/../xtensa-root/xtensa-byt-elf"
		HOST="xtensa-byt-elf"
		XTENSA_TOOLS_VERSION="RD-2012.5-linux"
	fi
	if [ $j == "cht" ]
	then
		PLATFORM="cherrytrail"
		XTENSA_CORE="CHT_audio_hifiep"
		ROOT="$pwd/../xtensa-root/xtensa-byt-elf"
		HOST="xtensa-byt-elf"
		XTENSA_TOOLS_VERSION="RD-2012.5-linux"
	fi
	if [ $j == "bdw" ]
	then
		PLATFORM="broadwell"
		ROOT="$pwd/../xtensa-root/xtensa-hsw-elf"
		HOST="xtensa-hsw-elf"
	fi
	if [ $j == "hsw" ]
	then
		PLATFORM="haswell"
		ROOT="$pwd/../xtensa-root/xtensa-hsw-elf"
		HOST="xtensa-hsw-elf"
	fi
	if [ $j == "apl" ]
	then
		PLATFORM="apollolake"
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
	fi
	if [ $j == "skl" ]
	then
		PLATFORM="skylake"
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
	fi
	if [ $j == "kbl" ]
	then
		PLATFORM="kabylake"
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
	fi
	if [ $j == "cnl" ]
	then
		PLATFORM="cannonlake"
		XTENSA_CORE="X6H3CNL_2016_4_linux"
		ROOT="$pwd/../xtensa-root/xtensa-cnl-elf"
		HOST="xtensa-cnl-elf"
		XTENSA_TOOLS_VERSION="RF-2016.4-linux"
	fi
	if [ $j == "sue" ]
        then
                PLATFORM="suecreek"
                XTENSA_CORE="X6H3CNL_2016_4_linux"
                ROOT="$pwd/../xtensa-root/xtensa-cnl-elf"
                HOST="xtensa-cnl-elf"
                XTENSA_TOOLS_VERSION="RF-2016.4-linux"
        fi
	if [ $j == "icl" ]
	then
		PLATFORM="icelake"
		XTENSA_CORE="X6H3CNL_2016_4_linux"
		ROOT="$pwd/../xtensa-root/xtensa-cnl-elf"
		HOST="xtensa-cnl-elf"
		XTENSA_TOOLS_VERSION="RF-2016.4-linux"
	fi
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
		fi
	fi

	# update ROOT directory for xt-xcc
	if [ "$XCC" == "xt-xcc" ]
	then
		ROOT="$XTENSA_BUILDS_DIR/$XTENSA_CORE/xtensa-elf"
		export XTENSA_SYSTEM=$XTENSA_BUILDS_DIR/$XTENSA_CORE/config
		PATH=$XTENSA_TOOLS_DIR/XtensaTools/bin:$OLDPATH
	else
		PATH=$pwd/../$HOST/bin:$OLDPATH
	fi

	# only delete binary related to this build
	rm -fr src/arch/xtensa/sof-$j.*

	./configure --with-arch=xtensa --with-platform=$PLATFORM \
		--with-root-dir=$ROOT --host=$HOST --enable-debug=$BUILD_DEBUG \
		CC=$XCC OBJCOPY=$XTOBJCOPY OBJDUMP=$XTOBJDUMP \
		--with-dsp-core=$XTENSA_CORE

	make clean
	make
	make bin
done

# list all the images
ls -l src/arch/xtensa/*.ri
