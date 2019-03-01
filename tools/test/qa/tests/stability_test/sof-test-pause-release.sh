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
# This script is used to test the ability of audio stream to pause and release.
# To do this, you need to install expect first.
# sudo apt-get install expect

# default

INTERVAL=0.5	# do pause/release per * second
FORMAT=s16_le	# sample format
CHL=2		# test channel number
FREQ=48000	# sample frequency
DURATION=300	# interrupt after # seconds

usage(){
	echo "usage: $0 -t pcmid -r frequency -f format -c channel \
-d duration -i interval audio_file"
	echo "eg: ./$0 -t pcm0p -r 48000 -f s16_le -c 2 -d 500 -i 1 audio.wav"
	exit
}

if [ $# == 0 ]; then
	echo "Please specify at least the PCM for testing."
	usage
else
	while getopts :ht:r:f:c:i:n:d: OPT
	do
	case $OPT in
		h)
		usage
		exit;;
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
			FREQ=$OPTARG;;
		f)
			FORMAT=$OPTARG;;
		c)
			CHL=$OPTARG;;
		i)
			INTERVAL=$OPTARG;;
		n)
			MAXLOOP=$OPTARG;;
		d)
			DURATION=$OPTARG;;
		?)
			usage
			exit;;
        esac
	done

	shift $(($OPTIND - 1))
	[[ ! -z "$1" ]] && data=$1
fi

[[ -z "$pcmid" ]] && usage
expect << EOF
spawn $cmd -Dhw:0,$pcmid -r $FREQ -f $FORMAT -c $CHL -d $DURATION -i -vv $data
expect {
	"Input/output" { send_user "error\n" }
	"#+*" {
	sleep $INTERVAL
	send " "
	exp_continue
	}
	"*PAUSE*" {
	sleep $INTERVAL
	send " ";
	exp_continue
	}
}
EOF
