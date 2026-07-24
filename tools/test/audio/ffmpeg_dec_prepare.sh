#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.
#
# Prepare FLAC test vectors and the reference PCM for verifying the SOF
# ffmpeg_dec module (see src/audio/ffmpeg_dec/). Produces, from a source WAV (or a
# generated test tone):
#
#   <out>/ref.wav            - the uncompressed source (ground truth PCM)
#   <out>/test.flac          - FLAC-encoded source
#   <out>/streaminfo.bin     - 34-byte FLAC STREAMINFO (the codec extradata blob
#                              to load into the module's bytes control)
#   <out>/stream.raw         - raw FLAC elementary stream to feed the decoder
#   <out>/ref_s32le.raw      - reference decoded PCM (interleaved S32_LE), the
#                              bit-exact target the module output must match
#
# FLAC is lossless, so a correct decode is bit-exact against ref_s32le.raw.
#
# Usage:
#   ffmpeg_dec_prepare.sh [-i input.wav] [-o outdir] [-r rate] [-c channels]
#
# Requires: ffmpeg, xxd (or od). Uses the host ffmpeg only for test-vector prep
# and the reference decode; the module under test is the on-target decoder.

set -e

IN=""
OUT="ffmpeg_dec_vectors"
RATE=48000
CH=2

while getopts "i:o:r:c:h" opt; do
	case $opt in
	i) IN=$OPTARG ;;
	o) OUT=$OPTARG ;;
	r) RATE=$OPTARG ;;
	c) CH=$OPTARG ;;
	h) grep '^#' "$0" | sed 's/^# \?//'; exit 0 ;;
	*) echo "bad option"; exit 1 ;;
	esac
done

command -v ffmpeg >/dev/null || { echo "ffmpeg not found"; exit 1; }
mkdir -p "$OUT"

# 1. Source PCM: use the provided WAV, else generate a 2s multi-tone test signal.
if [ -n "$IN" ]; then
	ffmpeg -y -loglevel error -i "$IN" -ar "$RATE" -ac "$CH" -c:a pcm_s16le "$OUT/ref.wav"
else
	echo "no -i input, generating a $RATE Hz ${CH}ch test tone"
	ffmpeg -y -loglevel error -f lavfi \
		-i "sine=frequency=997:sample_rate=$RATE:duration=2" \
		-ac "$CH" -c:a pcm_s16le "$OUT/ref.wav"
fi

# 2. Encode to FLAC.
ffmpeg -y -loglevel error -i "$OUT/ref.wav" -c:a flac "$OUT/test.flac"

# 3. Extract the 34-byte STREAMINFO metadata block (the codec extradata).
#    FLAC layout: "fLaC" (4B) then metadata blocks; the first block is always
#    STREAMINFO: 1B block header + 3B length (=34) + 34B STREAMINFO body.
#    The extradata FFmpeg expects is the 34-byte STREAMINFO body.
dd if="$OUT/test.flac" of="$OUT/streaminfo.bin" bs=1 skip=8 count=34 status=none
echo "streaminfo.bin: $(wc -c < "$OUT/streaminfo.bin") bytes (expect 34)"

# 4. Raw FLAC elementary stream to feed the decoder. The whole .flac works: the
#    libavcodec FLAC parser syncs on frame headers and skips the metadata. (A
#    frames-only variant would require parsing metadata block sizes.)
cp "$OUT/test.flac" "$OUT/stream.raw"

# 5. Reference decode to interleaved S32_LE PCM (bit-exact target).
ffmpeg -y -loglevel error -i "$OUT/test.flac" -f s32le -c:a pcm_s32le "$OUT/ref_s32le.raw"

echo "reference PCM: $(wc -c < "$OUT/ref_s32le.raw") bytes"
echo
echo "vectors written to $OUT/:"
ls -l "$OUT"
echo
echo "STREAMINFO (hex) to load into the module bytes control:"
if command -v xxd >/dev/null; then xxd "$OUT/streaminfo.bin"; else od -An -tx1 "$OUT/streaminfo.bin"; fi
