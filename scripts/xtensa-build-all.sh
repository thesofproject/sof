# fail on any errors
set -e

# run autogen.sh
./autogen.sh

pwd=`pwd`

# make sure rimage is built and aligned with code
./configure --enable-rimage
make
sudo make install

# now build the firmware (depends on rimage)
rm -fr src/arch/xtensa/*.ri

SUPPORTED_PLATFORMS=(byt cht bdw hsw apl cnl)

# build all images for chosen targets
if [ "$#" -eq 0 ]
then
	PLATFORMS=${SUPPORTED_PLATFORMS[@]}
else
	PLATFORMS=$@
fi

for i in ${PLATFORMS[@]}
do
	for j in ${SUPPORTED_PLATFORMS[@]}
	do
		if [ $j == $i ]
		then
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
		fi
	done
done

# list all the images
ls -l src/arch/xtensa/*.ri
