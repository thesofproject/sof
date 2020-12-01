#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# Utility script to pre-process and compile topology sources into topology test
# binaries. Currently supports simple PCM <-> component <-> SSP style tests
# using simple_test()

# Remove possible old topologies
rm -f test-*.conf test-*.tplg

# fail immediately on any errors
set -e

# M4 preprocessor flags
export M4PATH="../../topology/:../../topology/m4:../../topology/common:../../topology/platform/intel:../../topology/platform/common"

if [ -z "$SOF_TPLG_BUILD_OUTPUT" ]
then
      BUILD_OUTPUT="."
else
      BUILD_OUTPUT="$SOF_TPLG_BUILD_OUTPUT"
fi

# Simple component test cases
# can be used on components with 1 sink and 1 source.
SIMPLE_TESTS=(test-all test-capture test-playback)
TEST_STRINGS=""
M4_STRINGS=""
# process m4 simple tests -
# simple_test(name, pipe_name, be_name, format, dai_id, dai_format, dai_phy_bits, dai_data_bits dai_bclk)
# 1) name - test filename suffix
# 2) pipe_name - test component pipeline filename in sof/
# 3) be_name - BE DAI link name in machine driver, used for matching
# 4) format - PCM sample format
# 5) dai_type - dai type e.g. SSP/DMIC
# 6) dai_id - SSP port number
# 7) dai_format - SSP sample format
# 8) dai_phy_bits - SSP physical number of BLKCs per slot/channel
# 9) dai_data_bits - SSP number of valid data bits per slot/channel
# 10) dai_bclk - SSP BCLK in HZ
# 11) dai_mclk - SSP MCLK in HZ
# 12) SSP mode - SSP mode e.g. I2S, LEFT_J, DSP_A and DSP_B
# 13) SSP mclk_id
# 14) total number of pipelines - range from 1 to 4
# 15) Test pipelines
#

function simple_test {
	if [ $5 == "SSP" ]
	then
		TESTS=("${!15}")
	elif [ $5 == "DMIC" ]
	then
		TESTS=("${!15}")
	fi
	for i in ${TESTS[@]}
	do
		if [ $5 == "DMIC" ]
		then
			TFILE="$i-dmic$6-${14}-$2-$4-$7-$((${13} / 1000))k-$1"
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
				-DTEST_DMIC_SAMPLE_RATE=${13} \
				-DTEST_DMIC_PDM_CONFIG=${14} \
				-DTEST_DMIC_UNMUTE_TIME=400 \
				$i.m4 > "$BUILD_OUTPUT/${TFILE}.conf"
			echo "Compiling test $i -> $BUILD_OUTPUT/${TFILE}.tplg"
			alsatplg -v 1 -c "$BUILD_OUTPUT/${TFILE}.conf" -o "$BUILD_OUTPUT/${TFILE}.tplg"
		else
			if [ "$USE_XARGS" == "yes" ]
			then
				#if DAI type is SSP, define the SSP specific params
				if [ $5 == "SSP" ]
				then
					if [ $i == "test-all" ]
					then
						TFILE="test-ssp$6-mclk-${13}-${12}-$2-$4-$7-48k-$((${11} / 1000))k-$1"
					else
						if [ ${14} == "1" ]
						then
							TFILE="$i-ssp$6-mclk-${13}-${12}-$2-$4-$7-48k-$((${11} / 1000))k-$1"
						else
							TFILE="$i-ssp$6-mclk-${13}-${12}-${14}way-$2-$4-$7-48k-$((${11} / 1000))k-$1"
						fi
					fi
					#create input string for batch m4 processing
					M4_STRINGS+="-DTEST_PIPE_NAME=$2,-DTEST_DAI_LINK_NAME=$3\
						-DTEST_DAI_PORT=$6,-DTEST_DAI_FORMAT=$7\
						-DTEST_PIPE_FORMAT=$4,-DTEST_SSP_BCLK=${10}\
						-DTEST_SSP_MCLK=${11},-DTEST_SSP_PHY_BITS=$8\
						-DTEST_SSP_DATA_BITS=$9,-DTEST_SSP_MODE=${12}\
						-DTEST_SSP_MCLK_ID=${13},-DTEST_PIPE_AMOUNT=${14}\
						-DTEST_DAI_TYPE=$5\
						$i.m4,$BUILD_OUTPUT/${TFILE},"
					#create input string for batch processing of conf files
					TEST_STRINGS+="$BUILD_OUTPUT/${TFILE},"
				fi
			else
				#if DAI type is SSP, define the SSP specific params
				if [ $5 == "SSP" ]
				then
					if [ $i == "test-all" ]
					then
						TFILE="test-ssp$6-mclk-${13}-${12}-$2-$4-$7-48k-$((${11} / 1000))k-$1"
					else
						if [ ${14} == "1" ]
						then
							TFILE="$i-ssp$6-mclk-${13}-${12}-$2-$4-$7-48k-$((${11} / 1000))k-$1"
						else
							TFILE="$i-ssp$6-mclk-${13}-${12}-${14}way-$2-$4-$7-48k-$((${11} / 1000))k-$1"
						fi
					fi
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
						-DTEST_SSP_MCLK_ID=${13} \
						-DTEST_PIPE_AMOUNT=${14} \
						-DTEST_DAI_TYPE=$5 \
						$i.m4 > "$BUILD_OUTPUT/${TFILE}.conf"
					echo "Compiling test $i -> $BUILD_OUTPUT/${TFILE}.tplg"
					alsatplg -v 1 -c "$BUILD_OUTPUT/${TFILE}.conf" -o "$BUILD_OUTPUT/${TFILE}.tplg"
				fi
			fi
		fi
	done
}

echo "Preparing topology build input..."

# Pre-process the simple tests
simple_test nocodec passthrough "NoCodec-2" s16le SSP 2 s16le 20 16 1920000 19200000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec passthrough "NoCodec-2" s24le SSP 2 s24le 25 24 2400000 19200000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec-2" s16le SSP 2 s16le 20 16 1920000 19200000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec-2" s24le SSP 2 s24le 25 24 2400000 19200000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec-2" s16le SSP 2 s24le 25 24 2400000 19200000 I2S 0 1 SIMPLE_TESTS[@]

simple_test codec passthrough "SSP2-Codec" s16le SSP 2 s16le 20 16 1920000 19200000 I2S 0 1 SIMPLE_TESTS[@]
simple_test codec passthrough "SSP2-Codec" s24le SSP 2 s24le 25 24 2400000 19200000 I2S 0 1 SIMPLE_TESTS[@]
simple_test codec volume "SSP2-Codec" s16le SSP 2 s16le 20 16 1920000 19200000 I2S 0 1 SIMPLE_TESTS[@]
simple_test codec volume "SSP2-Codec" s24le SSP 2 s24le 25 24 2400000 19200000 I2S 0 1 SIMPLE_TESTS[@]
simple_test codec volume "SSP2-Codec" s24le SSP 2 s16le 20 16 1920000 19200000 I2S 0 1 SIMPLE_TESTS[@]
simple_test codec volume "SSP2-Codec" s16le SSP 2 s24le 25 24 2400000 19200000 I2S 0 1 SIMPLE_TESTS[@]

# for APL
APL_PROTOCOL_TESTS=(I2S LEFT_J DSP_A DSP_B)
APL_SSP_TESTS=(0 1 2 3 4 5)
APL_MODE_TESTS=(volume)
APL_FORMAT_TESTS=(s16le s24le s32le)
MCLK_IDS=(0 1)

for protocol in ${APL_PROTOCOL_TESTS[@]}
do
	for ssp in ${APL_SSP_TESTS[@]}
	do
		for mode in ${APL_MODE_TESTS[@]}
		do
			for format in ${APL_FORMAT_TESTS[@]}
			do
				for mclk_id in ${MCLK_IDS[@]}
				do
					simple_test nocodec $mode "NoCodec-${ssp}" $format SSP $ssp s16le 16 16 1536000 24576000 $protocol $mclk_id 1 SIMPLE_TESTS[@]
					simple_test nocodec $mode "NoCodec-${ssp}" $format SSP $ssp s24le 32 24 3072000 24576000 $protocol $mclk_id 1 SIMPLE_TESTS[@]
					simple_test nocodec $mode "NoCodec-${ssp}" $format SSP $ssp s32le 32 32 3072000 24576000 $protocol $mclk_id 1 SIMPLE_TESTS[@]

					simple_test codec $mode "SSP${ssp}-Codec" $format SSP $ssp s16le 16 16 1536000 24576000 $protocol $mclk_id 1 SIMPLE_TESTS[@]
					simple_test codec $mode "SSP${ssp}-Codec" $format SSP $ssp s24le 32 24 3072000 24576000 $protocol $mclk_id 1 SIMPLE_TESTS[@]
					simple_test codec $mode "SSP${ssp}-Codec" $format SSP $ssp s32le 32 32 3072000 24576000 $protocol $mclk_id 1 SIMPLE_TESTS[@]
				done
			done
		done
		for mclk_id in ${MCLK_IDS[@]}
		do
			simple_test nocodec passthrough "NoCodec-${ssp}" s16le SSP $ssp s16le 16 16 1536000 24576000 $protocol $mclk_id 1 SIMPLE_TESTS[@]
			simple_test nocodec passthrough "NoCodec-${ssp}" s24le SSP $ssp s24le 32 24 3072000 24576000 $protocol $mclk_id 1 SIMPLE_TESTS[@]
			simple_test nocodec passthrough "NoCodec-${ssp}" s32le SSP $ssp s32le 32 32 3072000 24576000 $protocol $mclk_id 1 SIMPLE_TESTS[@]

			simple_test codec passthrough "SSP${ssp}-Codec" s16le SSP $ssp s16le 16 16 1536000 24576000 $protocol $mclk_id 1 SIMPLE_TESTS[@]
			simple_test codec passthrough "SSP${ssp}-Codec"	s24le SSP $ssp s24le 32 24 3072000 24576000 $protocol $mclk_id 1 SIMPLE_TESTS[@]
			simple_test codec passthrough "SSP${ssp}-Codec"	s32le SSP $ssp s32le 32 32 3072000 24576000 $protocol $mclk_id 1 SIMPLE_TESTS[@]
		done
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
				simple_test nocodec $mode "NoCodec-${ssp}" $format SSP $ssp s16le 20 16 1920000 19200000 $protocol 0 1 SIMPLE_TESTS[@]
				simple_test nocodec $mode "NoCodec-${ssp}" $format SSP $ssp s24le 25 24 2400000 19200000 $protocol 0 1 SIMPLE_TESTS[@]

				simple_test codec $mode "SSP${ssp}-Codec" $format SSP $ssp s16le 20 16 1920000 19200000 $protocol 0 1 SIMPLE_TESTS[@]
				simple_test codec $mode "SSP${ssp}-Codec" $format SSP $ssp s24le 25 24 2400000 19200000 $protocol 0 1 SIMPLE_TESTS[@]
			done
		done
		simple_test nocodec passthrough "NoCodec-${ssp}" s16le SSP $ssp s16le 20 16 1920000 19200000 $protocol 0 1 SIMPLE_TESTS[@]
		simple_test nocodec passthrough "NoCodec-${ssp}" s24le SSP $ssp s24le 25 24 2400000 19200000 $protocol 0 1 SIMPLE_TESTS[@]

		simple_test codec passthrough "SSP${ssp}-Codec" s16le SSP $ssp s16le 20 16 1920000 19200000 $protocol 0 1 SIMPLE_TESTS[@]
		simple_test codec passthrough "SSP${ssp}-Codec" s24le SSP $ssp s24le 25 24 2400000 19200000 $protocol 0 1 SIMPLE_TESTS[@]
	done
done


# for processing algorithms
ALG_SINGLE_MODE_TESTS=(asrc eq-fir eq-iir src dcblock tdfb drc multiband-drc)
ALG_SINGLE_SIMPLE_TESTS=(test-capture test-playback)
ALG_MULTI_MODE_TESTS=(crossover)
ALG_MULTI_SIMPLE_TESTS=(test-playback)
ALG_MULTI_PIPE_AMOUNT=(2 3 4)
ALG_PROTOCOL_TESTS=(I2S)
ALG_SSP_TESTS=(5)
ALG_MCLK_IDS=(0)

for protocol in ${ALG_PROTOCOL_TESTS[@]}
do
	for ssp in ${ALG_SSP_TESTS[@]}
	do
		for mclk_id in ${ALG_MCLK_IDS[@]}
		do
			for mode in ${ALG_SINGLE_MODE_TESTS[@]}
			do
				simple_test codec $mode "SSP${ssp}-Codec" s16le SSP $ssp s16le 16 16 1536000 24576000 $protocol $mclk_id 1 ALG_SINGLE_SIMPLE_TESTS[@]
				simple_test codec $mode "SSP${ssp}-Codec" s24le SSP $ssp s24le 32 24 3072000 24576000 $protocol $mclk_id 1 ALG_SINGLE_SIMPLE_TESTS[@]
				simple_test codec $mode "SSP${ssp}-Codec" s32le SSP $ssp s32le 32 32 3072000 24576000 $protocol $mclk_id 1 ALG_SINGLE_SIMPLE_TESTS[@]
			done

			for mode in ${ALG_MULTI_MODE_TESTS[@]}
			do
				for pipe_num in ${ALG_MULTI_PIPE_AMOUNT[@]}
				do
					simple_test codec $mode "SSP${ssp}-Codec" s16le SSP $ssp s16le 16 16 1536000 24576000 $protocol $mclk_id $pipe_num ALG_MULTI_SIMPLE_TESTS[@]
					simple_test codec $mode "SSP${ssp}-Codec" s24le SSP $ssp s24le 32 24 3072000 24576000 $protocol $mclk_id $pipe_num ALG_MULTI_SIMPLE_TESTS[@]
					simple_test codec $mode "SSP${ssp}-Codec" s32le SSP $ssp s32le 32 32 3072000 24576000 $protocol $mclk_id $pipe_num ALG_MULTI_SIMPLE_TESTS[@]
				done
			done
		done
	done
done

# for CNL
simple_test nocodec passthrough "NoCodec-0" s16le SSP 0 s16le 25 16 2400000 24000000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec passthrough "NoCodec-2" s24le SSP 0 s24le 25 24 2400000 24000000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec-0" s16le SSP 0 s16le 25 16 2400000 24000000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec-0" s16le SSP 0 s24le 25 24 2400000 24000000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec-0" s24le SSP 0 s24le 25 24 2400000 24000000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec-0" s24le SSP 0 s16le 25 16 2400000 24000000 I2S 0 1 SIMPLE_TESTS[@]

simple_test nocodec passthrough "NoCodec-2" s16le SSP 2 s16le 25 16 2400000 24000000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec passthrough "NoCodec-2" s24le SSP 2 s24le 25 24 2400000 24000000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec-2" s16le SSP 2 s16le 25 16 2400000 24000000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec-2" s16le SSP 2 s24le 25 24 2400000 24000000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec-2" s24le SSP 2 s24le 25 24 2400000 24000000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec volume "NoCodec-2" s24le SSP 2 s16le 25 16 2400000 24000000 I2S 0 1 SIMPLE_TESTS[@]
simple_test nocodec src "NoCodec-4" s24le SSP 4 s24le 25 24 2400000 24000000 I2S 0 1 SIMPLE_TESTS[@]

# algorithms tests

if [ "$USE_XARGS" == "yes" ]
then
	printf '%s generating %s/*.conf files with m4...\n' "$0" "$BUILD_OUTPUT"
	M4_STRINGS=${M4_STRINGS%?}
	#m4 processing

	: ${NO_PROCESSORS:=$(nproc)}

	# Each test is defined by 15 consecutive strings, the last one
	# is the ${15}.conf output file.
	shell_name=m4 # $0, only used in error messages
	echo $M4_STRINGS | tr " " "," | tr '\n' '\0' |
	  xargs  ${VERBOSE:+-t} -P${NO_PROCESSORS} -d ',' -n15 \
	    bash ${VERBOSE:+-x} -c \
		'm4 --fatal-warnings "${@:1:${#}-1}" > ${15}.conf || exit 255' \
		$shell_name

	#execute alsatplg to create topology binary
	printf '%s generating %s/*.tplg files with alsatplg...\n' \
		"$0" "$BUILD_OUTPUT"
	TEST_STRINGS=${TEST_STRINGS%?}
	echo $TEST_STRINGS | tr '\n' ',' |
		xargs ${VERBOSE:+-t} -d ',' -P${NO_PROCESSORS} -n1 -I string \
		    alsatplg ${VERBOSE:+-v 1} -c string".conf" -o string".tplg"
fi
