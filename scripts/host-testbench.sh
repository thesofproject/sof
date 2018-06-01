#!/bin/sh

#build host library
sudo ./scripts/host-build-all.sh

#input file
input_file="48000Hz_stereo_16bit.raw"

#output_file
output_file="out.raw"

#input bit format
bits_in="S16_LE"

# topology file
# please use only simple volume topologies for now
topology_file="../sound-open-firmware-tools/topology/test/test-playback-ssp2-I2S-volume-s16le-s32le-48k-24576k-codec.tplg"

#optional libraries to override
libraries="vol=libsof_volume.so"

# Use -d to enable debug prints

# run testbench
./src/host/testbench -i $input_file -o $output_file -b $bits_in -t $topology_file -a $libraries -d
