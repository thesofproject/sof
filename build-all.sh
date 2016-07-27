# build images for all targets.


# build for Broxton
make clean
./configure --with-arch=xtensa --with-platform=broxton --with-tool-dir=~/source/reef/xtensa-bxt-elf --host=xtensa-bxt-elf
make
make bin

# Build for Baytrail
make clean
./configure --with-arch=xtensa --with-platform=baytrail --with-tool-dir=~/source/reef/xtensa-byt-elf --host=xtensa-byt-elf
make
make bin

# Build for Cherrytrail
make clean
./configure --with-arch=xtensa --with-platform=cherrytrail --with-tool-dir=~/source/reef/xtensa-byt-elf --host=xtensa-byt-elf
make
make bin

# Build for Broadwell
make clean
./configure --with-arch=xtensa --with-platform=broadwell --with-tool-dir=~/source/reef/xtensa-hsw-elf --host=xtensa-hsw-elf
make
make bin

# Build for Haswell
make clean
./configure --with-arch=xtensa --with-platform=haswell --with-tool-dir=~/source/reef/xtensa-hsw-elf --host=xtensa-hsw-elf
make
make bin
