#!/bin/bash

# Utility script to pre-process and compile topology sources into topology test
# binaries. Currently supports simple PCM <-> component <-> SSP style tests
# using simple_test()

# fail immediately on any errors
set -e

# M4 preprocessor flags
M4_FLAGS="-I ../ -I ../m4 -I ../common"

# Simple component test cases
# can be used on components with 1 sink and 1 source.
SIMPLE_TESTS=(test-ssp test-capture-ssp test-playback-ssp)
TONE_TEST=test-tone-playback-ssp


# process m4 simple tests -
# simple_test(name, pipe_name, be_name, format, dai_id, dai_format, dai_phy_bits, dai_data_bits dai_bclk)
# 1) name - test filename suffix
# 2) pipe_name - test component pipeline filename in sof/
# 3) be_name - BE DAI link name in machine driver, used for matching
# 4) format - PCM sample format
# 5) dai_id - SSP port number
# 6) dai_format - SSP sample format
# 7) dai_phy_bits - SSP physical number of BLKCs per slot/channel
# 8) dai_data_bits - SSP number of valid data bits per slot/channel
# 9) dai_bclk - SSP BCLK in HZ
# 10) dai_mclk - SSP MCLK in HZ
#
function simple_test {
	for i in ${SIMPLE_TESTS[@]}
	do
		TFILE="$i$5-$2-$4-$6-48k-$1"
		echo "M4 pre-processing test $i -> ${TFILE}"
		m4 ${M4_FLAGS} \
			-DTEST_PIPE_NAME="$2" \
			-DTEST_DAI_LINK_NAME="$3" \
			-DTEST_SSP_PORT=$5 \
			-DTEST_SSP_FORMAT=$6 \
			-DTEST_PIPE_FORMAT=$4 \
			-DTEST_SSP_BCLK=$9 \
			-DTEST_SSP_MCLK=${10} \
			-DTEST_SSP_PHY_BITS=$7 \
			-DTEST_SSP_DATA_BITS=$8 \
			$i.m4 > ${TFILE}.conf
		echo "Compiling test $i -> ${TFILE}.tplg"
		alsatplg -v 1 -c ${TFILE}.conf -o ${TFILE}.tplg
	done
}

# process m4 tone test -
# tone_test(name, pipe_name, be_name, format, dai_id, dai_format, dai_phy_bits, dai_data_bits dai_bclk)
# 1) name - test filename suffix
# 2) pipe_name - test component pipeline filename in sof/
# 3) be_name - BE DAI link name in machine driver, used for matching
# 4) format - PCM sample format
# 5) dai_id - SSP port number
# 6) dai_format - SSP sample format
# 7) dai_phy_bits - SSP physical number of BLKCs per slot/channel
# 8) dai_data_bits - SSP number of valid data bits per slot/channel
# 9) dai_bclk - SSP BCLK in HZ
# 10) dai_mclk - SSP MCLK in HZ
#
function tone_test {
	TFILE="$TONE_TEST$5-$2-$4-$6-48k-$1"
	echo "M4 pre-processing test $TONE_TEST -> ${TFILE}"
	m4 ${M4_FLAGS} \
		-DTEST_PIPE_NAME="$2" \
		-DTEST_DAI_LINK_NAME="$3" \
		-DTEST_SSP_PORT=$5 \
		-DTEST_SSP_FORMAT=$6 \
		-DTEST_PIPE_FORMAT=$4 \
		-DTEST_SSP_BCLK=$9 \
		-DTEST_SSP_MCLK=${10} \
		-DTEST_SSP_PHY_BITS=$7 \
		-DTEST_SSP_DATA_BITS=$8 \
		$TONE_TEST.m4 > ${TFILE}.conf
	echo "Compiling test $TONE_TEST -> ${TFILE}.tplg"
	alsatplg -v 1 -c ${TFILE}.conf -o ${TFILE}.tplg
}

# Pre-process the simple tests
simple_test nocodec passthrough "NoCodec" s16le 2 s16le 20 16 1920000 19200000
simple_test nocodec passthrough "NoCodec" s24le 2 s24le 25 24 2400000 19200000
simple_test nocodec volume "NoCodec" s16le 2 s16le 20 16 1920000 19200000
simple_test nocodec volume "NoCodec" s24le 2 s24le 25 24 2400000 19200000
simple_test nocodec volume "NoCodec" s16le 2 s24le 25 24 2400000 19200000
simple_test nocodec src "NoCodec" s24le 2 s24le 25 24 2400000 19200000

simple_test codec passthrough "SSP2-Codec" s16le 2 s16le 20 16 1920000 19200000
simple_test codec passthrough "SSP2-Codec" s24le 2 s24le 25 24 2400000 19200000
simple_test codec volume "SSP2-Codec" s16le 2 s16le 20 16 1920000 19200000
simple_test codec volume "SSP2-Codec" s24le 2 s24le 25 24 2400000 19200000
simple_test codec volume "SSP2-Codec" s16le 2 s24le 25 24 2400000 19200000
simple_test codec src "SSP2-Codec" s24le 2 s24le 25 24 2400000 19200000

simple_test baytrail passthrough "Baytrail Audio" s16le 2 s16le 20 16 1920000 19200000
simple_test baytrail passthrough "Baytrail Audio" s24le 2 s24le 25 24 2400000 19200000
simple_test baytrail volume "Baytrail Audio" s16le 2 s16le 20 16 1920000 19200000
simple_test baytrail volume "Baytrail Audio" s24le 2 s24le 25 24 2400000 19200000
simple_test baytrail volume "Baytrail Audio" s16le 2 s24le 25 24 2400000 19200000
simple_test baytrail src "Baytrail Audio" s24le 2 s24le 25 24 2400000 19200000

# for APL
simple_test nocodec volume "NoCodec" s16le 4 s16le 16 16 1536000 24576000
simple_test codec volume "SSP4-Codec" s16le 4 s16le 16 16 1536000 24576000
simple_test nocodec volume "NoCodec" s16le 5 s16le 16 16 1536000 24576000

# Tone test: Tone component only supports s32le currently
tone_test codec tone "SSP2-Codec" s32le 2 s16le 20 16 1920000 19200000


