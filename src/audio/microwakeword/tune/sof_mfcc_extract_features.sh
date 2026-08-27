#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# Batch-emit SOF mel40 MFCC feature files for a labelled WAV dataset using the
# SOF host testbench. Used for training microWakeWord (MWW) models against real
# SOF DSP frontend features.
#
# Expected input layout:
#   <wav_root>/<label>/<file>.wav      # any sample rate/channels; auto-converted
#
# Output layout:
#   <feat_root>/<label>/<file>.raw     # SOF mel40 hop records
#
# Each hop record is 184 bytes (matches the on-device wire format):
#   24 bytes  struct mfcc_data_header  (magic, frame_number, reserved,
#                                       energy, noise_energy, vad_flag)
#   40 x int32_t   Q9.23 mel-log values
#
# Requirements:
#   - $SOF_WORKSPACE points at the parent of the sof tree (or workspace root)
#   - sof-testbench4 built at tools/testbench/build_testbench/install/bin/
#   - sof-hda-benchmark-mfccmel40_10ms{16,24,32}.tplg built (development target)
#   - sox on PATH

set -e

usage() {
	cat >&2 <<EOF
Usage: $0 <wav_root> <feat_root> [--format S16|S24|S32] [--tplg <path>]

  wav_root    Directory containing <label>/*.wav (recursed one level).
  feat_root   Output root; <label>/ subdirs are created as needed.
  --format    Testbench sample container (S16, S24, S32). Default S32.
  --tplg      Explicit path to benchmark topology file.
EOF
	exit 1
}

[ $# -ge 2 ] || usage

WAV_ROOT="$1"
FEAT_ROOT="$2"
FORMAT="S32"
CUSTOM_TPLG=""

shift 2
while [ $# -gt 0 ]; do
	case "$1" in
		--format)
			FORMAT="$2"
			shift 2
			;;
		--tplg)
			CUSTOM_TPLG="$2"
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

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOF_ROOT="$(cd "$SCRIPT_DIR/../../../../" 2>/dev/null && pwd || true)"

if [[ -z "$SOF_WORKSPACE" ]]; then
	if [[ -d "$SOF_ROOT/tools/testbench" ]]; then
		SOF_WORKSPACE="$(cd "$SOF_ROOT/.." && pwd)"
	fi
fi

: "${SOF_WORKSPACE:?SOF_WORKSPACE must point at the parent of the sof tree}"

TESTBENCH="${SOF_WORKSPACE}/sof/tools/testbench/build_testbench/install/bin/sof-testbench4"
if [[ -n "$CUSTOM_TPLG" ]]; then
	TPLG="$CUSTOM_TPLG"
else
	TPLG="${SOF_WORKSPACE}/sof/tools/build_tools/topology/topology2/development/sof-hda-benchmark-mfccmel40_10ms${SF}.tplg"
fi

[ -x "$TESTBENCH" ] || { echo "Missing testbench: $TESTBENCH" >&2; exit 2; }
[ -f "$TPLG" ]     || { echo "Missing topology: $TPLG"       >&2; exit 2; }
[ -d "$WAV_ROOT" ] || { echo "Missing wav_root: $WAV_ROOT"   >&2; exit 2; }
command -v sox >/dev/null || { echo "sox not on PATH" >&2; exit 2; }

TMPDIR_LOCAL=$(mktemp -d -t sof_mfcc_mww.XXXXXX)
trap 'rm -rf "$TMPDIR_LOCAL"' EXIT

convert_wav() {
	local src="$1"
	local dst="$2"

	# Duplicate utterance with 100ms silence gap to warm up MFCC VAD noise floor
	case "$FORMAT" in
		S16) sox "$src" -p pad 0 0.10 | sox - "$src" -R --encoding signed-integer \
			-L -r 16000 -c 2 -b 16 "$dst" ;;
		S24) sox "$src" -p pad 0 0.10 | sox - "$src" -R --no-dither --encoding signed-integer \
			-L -r 16000 -c 2 -b 32 "$dst" vol 0.003906250000 ;;
		S32) sox "$src" -p pad 0 0.10 | sox - "$src" -R --no-dither --encoding signed-integer \
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
	local tb_log="${TMPDIR_LOCAL}/tb.log"

	# Calculate expected hop count for the single (original) utterance
	local orig_samples
	orig_samples=$(soxi -s "$wav" 2>/dev/null || echo 0)
	local orig_sr
	orig_sr=$(soxi -r "$wav" 2>/dev/null || echo 16000)
	local orig_hops=$(( (orig_samples * 16000 / orig_sr) / 160 ))

	mkdir -p "$out_dir"
	convert_wav "$wav" "$raw_in"
	echo "  extracting: ${base}.raw"

	if ! "$TESTBENCH" -d 0 \
		-r 16000 -c 2 -b "${FORMAT}_LE" -p 3,4 \
		-t "$TPLG" -i "$raw_in" -o "$raw_out" > "$tb_log" 2>&1; then
		cat "$tb_log" >&2
		echo "Error: testbench failed for $wav" >&2
		return 1
	fi

	# Retain only the trailing second-half hops where VAD noise floor is fully settled
	if [[ "$orig_hops" -gt 0 ]]; then
		python3 - "$raw_out" "$orig_hops" <<'PYEOF'
import sys
import numpy as np

raw_path = sys.argv[1]
orig_hops = int(sys.argv[2])

data = np.fromfile(raw_path, dtype=np.uint8)
if data.size >= 184:
    u32 = data[: (data.size // 4) * 4].view(np.uint32)
    pos = np.flatnonzero(u32 == 0x6D666363) * 4
    if len(pos) > orig_hops:
        start_byte = pos[-orig_hops]
        data[start_byte:].tofile(raw_path)
PYEOF
	fi
}

total=0
for label_dir in "$WAV_ROOT"/*/; do
	[ -d "$label_dir" ] || continue
	label=$(basename "$label_dir")
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
