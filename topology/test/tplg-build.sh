#!/bin/bash

# Utility script to pre-process and compile topology sources into topology test
# binaries. Currently supports simple PCM <-> component <-> SSP style tests
# using simple_test()

# fail immediately on any errors
set -e

# M4 preprocessor flags
export M4PATH="../:../m4:../common"

# Simple component test cases
# can be used on components with 1 sink and 1 source.
SIMPLE_TESTS=(test-all test-capture test-playback)
TONE_TEST=test-tone-playback
DMIC_TEST=test-capture
TEST_STRINGS=""
M4_STRINGS=""
# process m4 simple tests -
# simple_test(name, pipe_name, be_name, format, dai_id, dai_format, dai_phy_bits, dai_data_bits dai_bclk)
# 1) name - test filename suffix
# 2) pipe_name - test component pipeline filename in sof/
# 3) be_name - BE DAI link name in machine driver, used for matching
# 4) format - PCM sample format
# 5) dai_type - dai type e.g. SSP/DMIC
# 5) dai_id - SSP port number
# 6) dai_format - SSP sample format
# 7) dai_phy_bits - SSP physical number of BLKCs per slot/channel
# 8) dai_data_bits - SSP number of valid data bits per slot/channel
# 9) dai_bclk - SSP BCLK in HZ
# 10) dai_mclk - SSP MCLK in HZ
# 11) SSP mode - SSP mode e.g. I2S, LEFT_J, DSP_A and DSP_B
#

function simple_test {
	if [ $5 == "SSP" ]
	then
		TESTS=("${!13}")
	elif [ $5 == "DMIC" ]
	then
		TESTS=("${!15}")
	fi
	for i in ${TESTS[@]}
	do
		if [ $5 == "DMIC" ]
		then
			TFILE="$i-dmic$6-$2-$4-$7-48k-$1"
			echo "M4 pre-processing test $i -> ${TFILE}"
			m4 ${M4_FLAGS} \
				-DTEST_PIPE_NAME="$2" \
				-DTEST_DAI_LINK_NAME="$3" \
				-DTEST_DAI_PORT=$6 \
				-DTEST_DAI_FORMAT=$7 \
				-DTEST_PIPE_FORMAT=$4 \
				-DTEST_DAI_TYPE=$5 \
				-DTEST_DMIC_DRIVER_VERSION=$8 \
				-DTEST_DMIC_CLK_MIN=$9 \
				-DTEST_DMIC_CLK_MAX=${10} \
				-DTEST_DMIC_DUTY_MIN=${11} \
				-DTEST_DMIC_DUTY_MAX=${12} \
				-DTEST_PDM_ACTIVE=${13} \
				-DTEST_DMIC_SAMPLE_RATE=${14} \
				$i.m4 > ${TFILE}.conf
			echo "Compiling test $i -> ${TFILE}.tplg"
			alsatplg -v 1 -c ${TFILE}.conf -o ${TFILE}.tplg
		else
			if [ "$USE_XARGS" == "yes" ]
			then
				#if DAI type is SSP, define the SSP specific params
				if [ $5 == "SSP" ]
				then
					if [ $i == "test-all" ]
					then
						TFILE="test-ssp$6-${12}-$2-$4-$7-48k-$((${11} / 1000))k-$1"
					else
						TFILE="$i-ssp$6-${12}-$2-$4-$7-48k-$((${11} / 1000))k-$1"
					fi
					#create input string for batch m4 processing
					M4_STRINGS+="-DTEST_PIPE_NAME=$2,-DTEST_DAI_LINK_NAME=$3\
						-DTEST_DAI_PORT=$6,-DTEST_DAI_FORMAT=$7\
						-DTEST_PIPE_FORMAT=$4,-DTEST_SSP_BCLK=${10}\
						-DTEST_SSP_MCLK=${11},-DTEST_SSP_PHY_BITS=$8\
						-DTEST_SSP_DATA_BITS=$9,-DTEST_SSP_MODE=${12}\
						-DTEST_DAI_TYPE=$5\
						$i.m4,${TFILE},"
					#create input string for batch processing of conf files
					TEST_STRINGS+=${TFILE}","
				fi
			else
				#if DAI type is SSP, define the SSP specific params
				if [ $5 == "SSP" ]
				then
					echo "M4 pre-processing test $i -> ${TFILE}"
					m4 ${M4_FLAGS} \
						-DTEST_PIPE_NAME="$2" \
						-DTEST_DAI_LINK_NAME="$3" \
						-DTEST_DAI_PORT=$6 \
						-DTEST_DAI_FORMAT=$7 \
						-DTEST_PIPE_FORMAT=$4 \
						-DTEST_SSP_BCLK=${10} \
						-DTEST_SSP_MCLK=${11} \
						-DTEST_SSP_PHY_BITS=$8 \
						-DTEST_SSP_DATA_BITS=$9 \
						-DTEST_SSP_MODE=${12} \
						-DTEST_DAI_TYPE=$5 \
						$i.m4 > ${TFILE}.conf
					echo "Compiling test $i -> ${TFILE}.tplg"
					alsatplg -v 1 -c ${TFILE}.conf -o ${TFILE}.tplg
				fi
			fi
		fi
	done
}

echo "Preparing topology build input..."

# Pre-process the simple tests
simple_test nocodec passthrough "NoCodec" s16le SSP 2 s16le 20 16 1920000 19200000 I2S SIMPLE_TESTS[@]
simple_test nocodec passthrough "NoCodec" s24le SSP 2 s24le 25 24 2400000 19200000 I2S SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec" s16le SSP 2 s16le 20 16 1920000 19200000 I2S SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec" s24le SSP 2 s24le 25 24 2400000 19200000 I2S SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec" s16le SSP 2 s24le 25 24 2400000 19200000 I2S SIMPLE_TESTS[@]
simple_test nocodec src "NoCodec" s24le SSP 2 s24le 25 24 2400000 19200000 I2S SIMPLE_TESTS[@]

simple_test codec passthrough "SSP2-Codec" s16le SSP 2 s16le 20 16 1920000 19200000 I2S SIMPLE_TESTS[@]
simple_test codec passthrough "SSP2-Codec" s24le SSP 2 s24le 25 24 2400000 19200000 I2S SIMPLE_TESTS[@]
simple_test codec volume "SSP2-Codec" s16le SSP 2 s16le 20 16 1920000 19200000 I2S SIMPLE_TESTS[@]
simple_test codec volume "SSP2-Codec" s24le SSP 2 s24le 25 24 2400000 19200000 I2S SIMPLE_TESTS[@]
simple_test codec volume "SSP2-Codec" s24le SSP 2 s16le 20 16 1920000 19200000 I2S SIMPLE_TESTS[@]
simple_test codec volume "SSP2-Codec" s16le SSP 2 s24le 25 24 2400000 19200000 I2S SIMPLE_TESTS[@]
simple_test codec src "SSP2-Codec" s24le SSP 2 s24le 25 24 2400000 19200000 I2S SIMPLE_TESTS[@]

# for APL
APL_PROTOCOL_TESTS=(I2S LEFT_J DSP_A DSP_B)
APL_SSP_TESTS=(0 1 2 3 4 5)
APL_MODE_TESTS=(volume src)
APL_FORMAT_TESTS=(s16le s24le s32le)

for protocol in ${APL_PROTOCOL_TESTS[@]}
do
	for ssp in ${APL_SSP_TESTS[@]}
	do
		for mode in ${APL_MODE_TESTS[@]}
		do
			for format in ${APL_FORMAT_TESTS[@]}
			do
				simple_test nocodec $mode "NoCodec" $format SSP $ssp s16le 16 16 1536000 24576000 $protocol SIMPLE_TESTS[@]
				simple_test nocodec $mode "NoCodec" $format SSP $ssp s24le 32 24 3072000 24576000 $protocol SIMPLE_TESTS[@]
				simple_test nocodec $mode "NoCodec" $format SSP $ssp s32le 32 32 3072000 24576000 $protocol SIMPLE_TESTS[@]

				simple_test codec $mode "SSP${ssp}-Codec" $format SSP $ssp s16le 16 16 1536000 24576000 $protocol SIMPLE_TESTS[@]
				simple_test codec $mode "SSP${ssp}-Codec" $format SSP $ssp s24le 32 24 3072000 24576000 $protocol SIMPLE_TESTS[@]
				simple_test codec $mode "SSP${ssp}-Codec" $format SSP $ssp s32le 32 32 3072000 24576000 $protocol SIMPLE_TESTS[@]
			done
		done
		simple_test nocodec passthrough "NoCodec" s16le SSP $ssp s16le 16 16 1536000 24576000 $protocol SIMPLE_TESTS[@]
		simple_test nocodec passthrough "NoCodec" s24le SSP $ssp s24le 32 24 3072000 24576000 $protocol SIMPLE_TESTS[@]
		simple_test nocodec passthrough "NoCodec" s32le SSP $ssp s32le 32 32 3072000 24576000 $protocol SIMPLE_TESTS[@]

		simple_test codec passthrough "SSP${ssp}-Codec" s16le SSP $ssp s16le 16 16 1536000 24576000 $protocol SIMPLE_TESTS[@]
		simple_test codec passthrough "SSP${ssp}-Codec"	s24le SSP $ssp s24le 32 24 3072000 24576000 $protocol SIMPLE_TESTS[@]
		simple_test codec passthrough "SSP${ssp}-Codec"	s32le SSP $ssp s32le 32 32 3072000 24576000 $protocol SIMPLE_TESTS[@]
	done
done

for protocol in ${APL_PROTOCOL_TESTS[@]}
do
	for ssp in ${APL_SSP_TESTS[@]}
	do
		for mode in ${APL_MODE_TESTS[@]}
		do
			for format in ${APL_FORMAT_TESTS[@]}
			do
				simple_test nocodec $mode "NoCodec" $format SSP $ssp s16le 20 16 1920000 19200000 $protocol SIMPLE_TESTS[@]
				simple_test nocodec $mode "NoCodec" $format SSP $ssp s24le 25 24 2400000 19200000 $protocol SIMPLE_TESTS[@]

				simple_test codec $mode "SSP${ssp}-Codec" $format SSP $ssp s16le 20 16 1920000 19200000 $protocol SIMPLE_TESTS[@]
				simple_test codec $mode "SSP${ssp}-Codec" $format SSP $ssp s24le 25 24 2400000 19200000 $protocol SIMPLE_TESTS[@]
			done
		done
		simple_test nocodec passthrough "NoCodec" s16le SSP $ssp s16le 20 16 1920000 19200000 $protocol SIMPLE_TESTS[@]
		simple_test nocodec passthrough "NoCodec" s24le SSP $ssp s24le 25 24 2400000 19200000 $protocol SIMPLE_TESTS[@]

		simple_test codec passthrough "SSP${ssp}-Codec" s16le SSP $ssp s16le 20 16 1920000 19200000 $protocol SIMPLE_TESTS[@]
		simple_test codec passthrough "SSP${ssp}-Codec" s24le SSP $ssp s24le 25 24 2400000 19200000 $protocol SIMPLE_TESTS[@]
	done
done

# for CNL
simple_test nocodec passthrough "NoCodec" s16le SSP 2 s16le 25 16 2400000 24000000 I2S SIMPLE_TESTS[@]
simple_test nocodec passthrough "NoCodec" s24le SSP 2 s24le 25 24 2400000 24000000 I2S SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec" s16le SSP 2 s16le 25 16 2400000 24000000 I2S SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec" s16le SSP 2 s24le 25 24 2400000 24000000 I2S SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec" s24le SSP 2 s24le 25 24 2400000 24000000 I2S SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec" s24le SSP 2 s16le 25 16 2400000 24000000 I2S SIMPLE_TESTS[@]
simple_test nocodec src "NoCodec" s24le SSP 4 s24le 25 24 2400000 24000000 I2S SIMPLE_TESTS[@]

# Tone test: Tone component only supports s32le currently
simple_test codec tone "SSP2-Codec" s32le SSP 2 s16le 20 16 1920000 19200000 I2S TONE_TEST[@]

#DMIC Test
simple_test nocodec passthrough "DMIC0" s32le DMIC 0 s32le 1 500000 4800000\
	40 60 1 48000 DMIC_TEST[@]

if [ "$USE_XARGS" == "yes" ]
then
	echo "Batch processing m4 files..."
	M4_STRINGS=${M4_STRINGS%?}
	#m4 processing
	echo $M4_STRINGS | tr " " "," | tr '\n' '\0' | xargs -P0 -d ',' -n13 bash -c 'm4 "${@:1:${#}-1}" > ${13}.conf' m4

	#execute alsatplg to create topology binary
	TEST_STRINGS=${TEST_STRINGS%?}
	echo $TEST_STRINGS | tr '\n' ',' |\
		xargs -d ',' -P0 -n1 -I string alsatplg -v 1 -c\
			string".conf" -o string".tplg"
fi

