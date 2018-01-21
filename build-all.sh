# fail on any errors
set -e

# run autogen.sh
./autogen.sh

# build all images for all targets.
pwd=`pwd`

rm -fr src/arch/xtensa/*.ri

# Build for Baytrail
./configure --with-arch=xtensa --with-platform=baytrail --with-root-dir=$pwd/../xtensa-root/xtensa-byt-elf --host=xtensa-byt-elf
make clean
make
make bin

# Build for Cherrytrail
make clean
./configure --with-arch=xtensa --with-platform=cherrytrail --with-root-dir=$pwd/../xtensa-root/xtensa-byt-elf --host=xtensa-byt-elf
make
make bin

# Build for Broadwell
make clean
./configure --with-arch=xtensa --with-platform=broadwell --with-root-dir=$pwd/../xtensa-root/xtensa-hsw-elf --host=xtensa-hsw-elf
make
make bin

# Build for Haswell
make clean
./configure --with-arch=xtensa --with-platform=haswell --with-root-dir=$pwd/../xtensa-root/xtensa-hsw-elf --host=xtensa-hsw-elf
make
make bin


# list all the images
ls -l src/arch/xtensa/*.ri
