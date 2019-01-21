#!/bin/bash

# Copyright (c) 2019, Intel Corporation
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
# This script is used to test the ability of alsa xrun handling. To do this
# you need to enable the xrun debugging in kernel driver first.
# +CONFIG_SND_DEBUG=y
# +CONFIG_SND_DEBUG_VERBOSE=y
# +CONFIG_SND_PCM_XRUN_DEBUG=y

FORMAT=s16_le	# sample format
CHANNEL=2	# test channel number
FREQUENCY=48000	# sample frequency
INTERVAL=0.2	# interval time for each xrun injection
MAXLOOP=10000	# max loop for xrun injection

usage(){
	echo "usage: $0 -t pcmid -r frequency -f format -c channel\
-i interval -n loops -d audio_file"
	echo "eg: ./$0 -t pcm0p -r 48000 -f s16_le -c 2 -i 0.2\
-n 1000 -d audio.wav"
	exit
}

xrun_inject()
{
	int=0
	while [ $int -lt $MAXLOOP ]
	do
		echo "XRUN injection: $int"
		echo 1 > /proc/asound/card0/$t_pcm/sub0/xrun_injection
		sleep $INTERVAL
		let int++
		# check the pcm status
		num=`ps -ef |grep "$PID"| grep -v "grep" |wc -l`
		if [ $num == 0 ]; then
			echo "$cmd is finished, stop the xrun injection"
			exit 1
	fi
done
}

if [ $# == 0 ]; then
	echo "Please specify the PCM to test."
               usage
else
	while getopts :t:r:f:c:i:n:d OPT
	do
	case $OPT in
		t)
			t_pcm=$OPTARG
			pcmid=`echo $t_pcm |tr -cd "[0-9]"`
			test_type=`echo ${t_pcm: -1}`
			if [[ $test_type == 'p' || $test_type == "P" ]]; then
				cmd=aplay
				data=/dev/zero
			elif [[ $test_type == 'c' || $test_type == "C" ]]; then
				cmd=arecord
				data=/dev/null
			fi
			;;
		r)
			FREQUENCY=$OPTARG;;
		f)
			FORMAT=$OPTARG;;
		c)
			CHANNEL=$OPTARG;;
		i)
			INTERVAL=$OPTARG;;
		n)
			MAXLOOP=$OPTARG;;
		d)
			data=$OPTARG;;
		*)
			usage;;
	esac
	done
fi

[[ -z "$pcmid" ]] && usage
$cmd -Dhw:0,$pcmid -f $FORMAT -r $FREQUENCY -c $CHANNEL $data &
PID=$!

# do xrun injection
xrun_inject

# stop the pcm after xrun injection is done
kill $PID
echo "XRUN injection test is done"
