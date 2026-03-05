#!/bin/bash

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <input_file.wav/flac/etc.> <output_file.wav>"
    exit
fi

CLIP_IN=$1
WAV_OUT=$2
TESTBENCH=$SOF_WORKSPACE/sof/tools/testbench/build_testbench/install/bin/sof-testbench4
TPLG_DIR=$SOF_WORKSPACE/sof/tools/build_tools/topology/topology2/development
TPLG=/sof-hda-benchmark-phase_vocoder32.tplg
RAW_IN=$(mktemp --suffix _in.raw)
RAW_OUT=$(mktemp --suffix _out.raw)
CONTROL_SCRIPT_LONG=/tmp/testbench_controls_full.sh
CONTROL_SCRIPT="-s $CONTROL_SCRIPT_LONG"
#CONTROL_SCRIPT=
#VALGRIND="valgrind --leak-check=full --track-origins=yes"
VALGRIND=

cat <<EOF_LONG > $CONTROL_SCRIPT_LONG
#!/bin/sh
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 0.5
amixer -c0 cset name='Analog Playback Phase Vocoder enable' on
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 0.6
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 0.7
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 0.8
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 0.9
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.0
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.1
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.2
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.3
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.4
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.5
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.6
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.7
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.8
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.9
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 2.0
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.9
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.8
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.7
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.6
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.5
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.4
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.3
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.2
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.1
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder speed' 1.0
sleep 1
amixer -c0 cset name='Analog Playback Phase Vocoder enable' off
EOF_LONG

sox "$CLIP_IN" --encoding signed-integer -L -r 48000 -c 2 -b 32 "$RAW_IN"
$VALGRIND "$TESTBENCH" $CONTROL_SCRIPT -r 48000 -c 2 -b S32_LE -p 1,2 \
	  -t "$TPLG_DIR"/$TPLG -i "$RAW_IN" -o "$RAW_OUT"
sox --encoding signed-integer -L -r 48000 -c 2 -b 32 "$RAW_OUT" "$WAV_OUT"
rm "$RAW_IN"
rm "$RAW_OUT"

echo; file "$WAV_OUT"
