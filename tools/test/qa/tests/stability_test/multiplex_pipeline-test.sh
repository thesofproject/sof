#!/bin/bash
# Simple script for simultaneous multiplex pipeline testing

# Copyright (c) 2016, Intel Corporation
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#   * Neither the name of the Intel Corporation nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# Author: Keqiao Zhang <keqiao.zhang@linux.intel.com>
#

NUMBER=$#	# number of test pipeline
PARAMS=$@	# name of test pipeline
MAXLOOPS=1000	# loop number
FORMATE=s16_le	# sample formate
CHANNEL=2	# test channel number
FREQENCY=48000	# sample frequency
T_TIME=3	# time for each test
INTERVAL=0.5	# internal time for each cycle


pipeline_test()
{
	index=0
	# start pipeline
	for i in $PARAMS
	do
		arg=$i
		pcmid=`echo $arg| cut -c 4`
		test_type=`echo $arg| cut -c 5`
		if [[ $test_type == 'p' || $test_type == "P" ]]; then
			cmd=aplay
			data=/dev/zero
		elif [[ $test_type == 'c' || $test_type == "C" ]]; then
			cmd=arecord
			data=/dev/null
		else
			echo "Wrong parameters, should be pcm0p/pcm0c ..."
			exit 1
		fi

		# run pipeline
		$cmd -Dhw:0,$pcmid -f $FORMATE -c $CHANNEL -r $FREQENCY $data &
		PID[$index]=$!
		let index++
	done

	sleep $T_TIME

	# stop pipeline
	for i in ${PID[*]}
	do
		kill ${i}
		if [ $? -ne 0 ];then
			echo "Stop pipeline failure."
			exit 1
		fi
	done
}

# run test
if [ $# == 0 ]; then
	echo "Usage:./multiplex-pipeline-test.sh '\$pipeline1' '\$pipeline2'.."
	echo "eg. ./multiple-pipeline-test.sh pcm1p pcm1c"
	echo "it will check playback and capture on pcm1 at same time"
	exit 1
fi

for i in $(seq 1 $MAXLOOPS)
do
	echo "Test: $i"
	pipeline_test
	sleep $INTERVAL
done
