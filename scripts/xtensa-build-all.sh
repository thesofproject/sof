#!/bin/bash

SUPPORTED_PLATFORMS=(byt cht bdw hsw apl cnl)
if [ "$#" -eq 0 ]
then
	PLATFORMS=${SUPPORTED_PLATFORMS[@]}
else
	# parse the args
	for args in $@
	do
		if [[ "$args" == "-l" ]]
			then
			BUILD_LOCAL=1

			# build all images for chosen targets
			if [ "$#" -eq 1 ]
			then
				PLATFORMS=${SUPPORTED_PLATFORMS[@]}
				break
			fi
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


# now build the firmware (depends on rimage)
rm -fr src/arch/xtensa/*.ri

# fail on any errors
set -e

# run autogen.sh
./autogen.sh

pwd=`pwd`


# make sure rimage is built and aligned with code
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

# build platform
for j in ${PLATFORMS[@]}
do
	if [ $j == "byt" ]
	then
		PLATFORM="baytrail"
		ROOT="xtensa-byt-elf"
	fi
	if [ $j == "cht" ]
	then
		PLATFORM="cherrytrail"
		ROOT="xtensa-byt-elf"
	fi
	if [ $j == "bdw" ]
	then
		PLATFORM="broadwell"
		ROOT="xtensa-hsw-elf"
	fi
	if [ $j == "hsw" ]
	then
		PLATFORM="haswell"
		ROOT="xtensa-hsw-elf"
	fi
	if [ $j == "apl" ]
	then
		PLATFORM="apollolake"
		ROOT="xtensa-bxt-elf"
	fi
	if [ $j == "cnl" ]
	then
		PLATFORM="cannonlake"
		ROOT="xtensa-cnl-elf"
	fi
	PATH=$pwd/../xtensa-root/$ROOT/bin:$PATH
	./configure --with-arch=xtensa --with-platform=$PLATFORM --with-root-dir=$pwd/../xtensa-root/$ROOT --host=$ROOT
	make clean
	make
	make bin
done

# list all the images
ls -l src/arch/xtensa/*.ri
