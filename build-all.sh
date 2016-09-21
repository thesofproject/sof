# build images for all targets.

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

