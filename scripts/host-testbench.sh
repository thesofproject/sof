#!/bin/sh

#build host library
sudo ./scripts/host-build-all.sh

#input file
input_file="48000Hz_stereo_16bit.raw"

#output_file
output_file="out.raw"

#input bit format
bits_in="S16_LE"

#input sample rate (this is an optional argument for SRC based pipelines)
#should be used with -r option
fs_in="48000"

#output sample rate (this is an optional argument for SRC based pipelines)
#should be used with -R option
fs_out="96000"

# topology file
# please use simple volume/src topologies for now

topology_file="../soft.git/topology/test/test-playback-ssp2-I2S-volume-s16le-s32le-48k-24576k-codec.tplg"

#example src topology
#topology_file="../soft.git/topology/test/test-playback-ssp5-LEFT_J-src-s24le-s24le-48k-19200k-codec.tplg"

#optional libraries to override
libraries="vol=libsof_volume.so,src=libsof_src.so"

# Use -d to enable debug prints

# run volume testbench
./src/host/testbench -i $input_file -o $output_file -b $bits_in -t $topology_file -a $libraries -d

# run src testbench
#./src/host/testbench -i $input_file -o $output_file -b $bits_in -t $topology_file -a $libraries -r $fs_in -R $fs_out -d
