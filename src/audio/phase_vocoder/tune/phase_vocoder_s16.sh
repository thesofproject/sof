#!/bin/bash
set -euo pipefail

if [ $# -lt 2 ]; then
    echo "Usage: $0 <input_file.wav/flac/etc.> <output_file.wav>" >&2
    exit 1
fi

: "${SOF_WORKSPACE:?SOF_WORKSPACE must be set}"

CLIP_IN=$1
WAV_OUT=$2
TESTBENCH=$SOF_WORKSPACE/sof/tools/testbench/build_testbench/install/bin/sof-testbench4
TPLG_DIR=$SOF_WORKSPACE/sof/tools/build_tools/topology/topology2/development
TPLG=sof-hda-benchmark-phase_vocoder16.tplg

RAW_IN=$(mktemp --suffix=_in.raw)
RAW_OUT=$(mktemp --suffix=_out.raw)
CONTROL_SCRIPT_LONG=$(mktemp --suffix=_controls.sh)
trap 'rm -f "$RAW_IN" "$RAW_OUT" "$CONTROL_SCRIPT_LONG"' EXIT

# Uncomment to run under valgrind:
# VALGRIND=(valgrind --leak-check=full --track-origins=yes)
VALGRIND=()
CONTROL_SCRIPT=(-s "$CONTROL_SCRIPT_LONG")

# Generated script is consumed by the testbench's -s option, not executed
# directly by a shell.
{
    echo "#!/bin/sh"
    echo "amixer -c0 cset name='Analog Playback Phase Vocoder speed' 0.5"
    echo "amixer -c0 cset name='Analog Playback Phase Vocoder enable' on"
    for s in 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 \
             2.0 1.9 1.8 1.7 1.6 1.5 1.4 1.3 1.2 1.1 1.0; do
        echo "sleep 1"
        echo "amixer -c0 cset name='Analog Playback Phase Vocoder speed' $s"
    done
    echo "amixer -c0 cset name='Analog Playback Phase Vocoder enable' off"
} > "$CONTROL_SCRIPT_LONG"

sox "$CLIP_IN" --encoding signed-integer -L -r 48000 -c 2 -b 16 "$RAW_IN"
"${VALGRIND[@]}" "$TESTBENCH" "${CONTROL_SCRIPT[@]}" \
    -r 48000 -c 2 -b S16_LE -p 1,2 \
    -t "$TPLG_DIR/$TPLG" -i "$RAW_IN" -o "$RAW_OUT"
sox --encoding signed-integer -L -r 48000 -c 2 -b 16 "$RAW_OUT" "$WAV_OUT"

printf '\n'
file "$WAV_OUT"
