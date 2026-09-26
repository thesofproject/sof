#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# Batch-emit SOF mel40 MFCC feature files for a labelled WAV dataset, for
# retraining the TFLM micro_speech model against real SOF features.
#
# Expected input layout:
#   <wav_root>/<label>/<file>.wav      # any sample rate/channels; auto-converted
#
# Output layout:
#   <feat_root>/<label>/<file>.raw     # SOF mel40 hop records, one after another
#
# Each hop record is 184 bytes (matches the on-device wire format):
#   24 bytes  struct mfcc_data_header  (magic, frame_number, reserved,
#                                       energy, noise_energy, vad_flag)
#   40 x int32_t   Q9.23 mel-log values
#
# The consumer (Python training loader) parses N=hop_count records, strips the
# 24-byte headers, and reshapes to (N, 40) int32 Q9.23. See decode_mel.m for a
# reference decoder.
#
# Level-jitter augmentation happens upstream in the WAV generators
# (sof_tflm_generate_keyword_dataset.sh, sof_tflm_prepare_silence_unknown.sh)
# so it's audible on disk. This script just consumes whatever WAVs are
# already in <wav_root>/<label>/.
#
# Requirements:
#   - $SOF_WORKSPACE points at the parent of the sof tree
#   - sof-testbench4 built at tools/testbench/build_testbench/install/bin/
#   - sof-hda-benchmark-mfccmel40{16,24,32}.tplg built (development target)
#   - sox on PATH

set -e

usage() {
	cat >&2 <<EOF
Usage: $0 <wav_root> <feat_root> [--format S16|S24|S32]

  wav_root    Directory containing <label>/*.wav (recursed one level).
  feat_root   Output root; <label>/ subdirs are created as needed.
  --format    Testbench sample container. Default S32 (matches on-device).
EOF
	exit 1
}

[ $# -ge 2 ] || usage

WAV_ROOT="$1"
FEAT_ROOT="$2"
FORMAT="S32"

shift 2
while [ $# -gt 0 ]; do
	case "$1" in
	--format)
		FORMAT="$2"
		shift 2
		;;
	*)
		usage
		;;
	esac
done

case "$FORMAT" in
S16) SF="16" ;;
S24) SF="24" ;;
S32) SF="32" ;;
*) usage ;;
esac

: "${SOF_WORKSPACE:?SOF_WORKSPACE must point at the parent of the sof tree}"

TESTBENCH="${SOF_WORKSPACE}/sof/tools/testbench/build_testbench/install/bin/sof-testbench4"
TPLG="${SOF_WORKSPACE}/sof/tools/build_tools/topology/topology2/development/sof-hda-benchmark-mfccmel40${SF}.tplg"

[ -x "$TESTBENCH" ] || { echo "Missing testbench: $TESTBENCH" >&2; exit 2; }
[ -f "$TPLG" ]     || { echo "Missing topology: $TPLG"       >&2; exit 2; }
[ -d "$WAV_ROOT" ] || { echo "Missing wav_root: $WAV_ROOT"   >&2; exit 2; }
command -v sox >/dev/null || { echo "sox not on PATH" >&2; exit 2; }

TMPDIR_LOCAL=$(mktemp -d -t sof_mfcc_train.XXXXXX)
trap 'rm -rf "$TMPDIR_LOCAL"' EXIT

# sox parameters: 16 kHz stereo (bench topology expects -c 2 -p 3,4).
# MFCC config picks channel 0 (see get_mel_spectrogram_config in setup_mfcc.m),
# so a mono WAV upmixed to stereo is fine.
convert_wav() {
	local src="$1"
	local dst="$2"

	case "$FORMAT" in
	S16) sox -R --encoding signed-integer "$src" \
		-L -r 16000 -c 2 -b 16 "$dst" ;;
	S24) sox -R --no-dither --encoding signed-integer "$src" \
		-L -r 16000 -c 2 -b 32 "$dst" vol 0.003906250000 ;;
	S32) sox -R --no-dither --encoding signed-integer "$src" \
		-L -r 16000 -c 2 -b 32 "$dst" ;;
	esac
}

process_wav() {
	local wav="$1"
	local out_dir="$2"
	local base
	base=$(basename "$wav" .wav)

	local raw_in="${TMPDIR_LOCAL}/in.raw"
	local raw_out="${out_dir}/${base}.raw"

	mkdir -p "$out_dir"
	convert_wav "$wav" "$raw_in"

	# -d 0 silences testbench traces (there is no -q flag).
	"$TESTBENCH" -d 0 \
		-r 16000 -c 2 -b "${FORMAT}_LE" -p 3,4 \
		-t "$TPLG" -i "$raw_in" -o "$raw_out"
}

total=0
for label_dir in "$WAV_ROOT"/*/; do
	[ -d "$label_dir" ] || continue
	label=$(basename "$label_dir")
	# Skip underscore-prefixed staging dirs (e.g. _raw from the Piper generator).
	case "$label" in _*) continue ;; esac
	out_dir="${FEAT_ROOT}/${label}"

	echo "[label=${label}]"
	count=0
	for wav in "$label_dir"*.wav; do
		[ -f "$wav" ] || continue
		process_wav "$wav" "$out_dir"
		count=$((count + 1))
	done
	echo "  wrote ${count} feature files to ${out_dir}"
	total=$((total + count))
done

echo "-----------------------------------------------------------------"
echo "Emitted ${total} mel40 feature files under ${FEAT_ROOT}"
echo "Format per hop: 24-byte mfcc_data_header + 40 x int32 Q9.23 = 184 bytes"
echo "-----------------------------------------------------------------"
