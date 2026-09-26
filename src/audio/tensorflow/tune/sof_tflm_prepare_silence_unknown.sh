#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# Prepare the silence/ and unknown/ dataset classes for TFLM wake-on-voice
# training by fetching Google Speech Commands v2 and slicing/sampling from it.
#
# Output layout (merges with any existing keyword class dirs under <out_root>):
#   <out_root>/silence/*.wav   1-second slices of _background_noise_/*.wav
#   <out_root>/unknown/*.wav   random sample of non-target Speech Commands words
#
# Speech Commands v2 is 2.4 GB (~106k files). It is cached under $SC_CACHE
# (default ~/.cache/speech_commands_v2) so subsequent runs are instant.
#
# Requirements: sox on PATH, ~3 GB of free disk for the cache.

set -e

OUT_ROOT="${1:-}"
if [[ -z "$OUT_ROOT" ]]; then
	cat >&2 <<EOF
Usage: $0 <out_root>

  out_root   Dataset root; silence/ and unknown/ are added under it, alongside
             any keyword class directories already present.

Optional env:
  SC_CACHE          Directory holding the unpacked Speech Commands v2 tree
                    (default: ~/.cache/speech_commands_v2).
  N_SILENCE         Number of 1-second silence clips to emit (default 500).
  N_UNKNOWN         Number of unknown-class clips to sample (default 1500).
  UNKNOWN_WORDS     Space-separated word list to sample for unknown
                    (default: every SC v2 word except _background_noise_).
  GAIN_AUG          1 = rewrite each unknown/*.wav in place with a peak-
                    normalized target and per-file Gaussian level jitter
                    (silence/ is left alone so background noise stays
                    quiet). 0 = leave the clips at their sox slice level.
                    Default 0.
  GAIN_PEAK_DBFS    Peak-normalization target in dBFS (default -10).
  GAIN_SIGMA_DB     Gaussian jitter sigma in dB (default 5).
  GAIN_HEADROOM_DB  Headroom in dB below full scale that the augmented peak
                    must respect. Positive offsets are capped so
                    peak+offset <= -GAIN_HEADROOM_DB, keeping sox out of
                    the dither/gain clip warning path (default 1).
EOF
	exit 1
fi

: "${SC_CACHE:=$HOME/.cache/speech_commands_v2}"
: "${N_SILENCE:=500}"
: "${N_UNKNOWN:=1500}"
: "${GAIN_AUG:=0}"
: "${GAIN_PEAK_DBFS:=-10}"
: "${GAIN_SIGMA_DB:=5}"
: "${GAIN_HEADROOM_DB:=1}"

gauss_offset_db() {
	local sigma="$1"
	local peak="$2"
	local headroom="$3"
	awk -v s="$sigma" \
	    -v cap="$(awk -v p="$peak" -v h="$headroom" 'BEGIN{print -p-h}')" \
	    -v seed="$RANDOM$(date +%N)" 'BEGIN {
		srand(seed)
		u1 = rand(); if (u1 < 1e-12) u1 = 1e-12
		u2 = rand()
		v = s * sqrt(-2 * log(u1)) * cos(6.283185307 * u2)
		if (v > cap) v = cap
		printf "%.3f", v
	}'
}

apply_gain_jitter() {
	local dir="$1"
	local sum=0 n=0 mn=999 mx=-999 off tmp
	tmp=$(mktemp --suffix=.wav)
	for wav in "$dir"/*.wav; do
		[ -f "$wav" ] || continue
		off=$(gauss_offset_db "$GAIN_SIGMA_DB" "$GAIN_PEAK_DBFS" "$GAIN_HEADROOM_DB")
		sox "$wav" "$tmp" gain -n "$GAIN_PEAK_DBFS" gain "$off"
		mv "$tmp" "$wav"
		sum=$(awk -v a="$sum" -v b="$off" 'BEGIN{printf "%.3f", a+b}')
		mn=$(awk -v a="$mn" -v b="$off" 'BEGIN{print (b<a)?b:a}')
		mx=$(awk -v a="$mx" -v b="$off" 'BEGIN{print (b>a)?b:a}')
		n=$((n + 1))
	done
	rm -f "$tmp"
	if [ "$n" -gt 0 ]; then
		awk -v n="$n" -v s="$sum" -v mn="$mn" -v mx="$mx" \
		    -v p="$GAIN_PEAK_DBFS" -v h="$GAIN_HEADROOM_DB" 'BEGIN{
			printf "    gain jitter n=%d  offset mean=%.2f dB  min=%.2f  max=%.2f  target peak=%s dBFS  ceiling=%s dBFS\n", n, s/n, mn, mx, p, -h
		}'
	fi
}

if ! command -v sox >/dev/null; then
	echo "sox not on PATH — please install (e.g. apt install sox)" >&2
	exit 1
fi

# Fetch + unpack once.
if [[ ! -d "$SC_CACHE/_background_noise_" ]]; then
	echo ">>> Fetching Speech Commands v2 into $SC_CACHE (~2.4 GB, one-time)"
	mkdir -p "$SC_CACHE"
	url="http://download.tensorflow.org/data/speech_commands_v0.02.tar.gz"
	tarball="$SC_CACHE/speech_commands_v0.02.tar.gz"
	if [[ ! -f "$tarball" ]]; then
		wget -O "$tarball" "$url"
	fi
	tar -C "$SC_CACHE" -xzf "$tarball"
fi

SILENCE_OUT="$OUT_ROOT/silence"
UNKNOWN_OUT="$OUT_ROOT/unknown"
mkdir -p "$SILENCE_OUT" "$UNKNOWN_OUT"

# --- silence class ---------------------------------------------------------
# Each _background_noise_/*.wav is ~60 s. Slice into 1 s chunks at random
# offsets to build up the target count; sox trim handles this cleanly.
echo ">>> Producing $N_SILENCE silence clips into $SILENCE_OUT"
mapfile -t noise_wavs < <(find "$SC_CACHE/_background_noise_" -name '*.wav')
if [[ ${#noise_wavs[@]} -eq 0 ]]; then
	echo "No _background_noise_ wavs found in $SC_CACHE" >&2
	exit 1
fi
i=0
while [[ $i -lt $N_SILENCE ]]; do
	src="${noise_wavs[$((RANDOM % ${#noise_wavs[@]}))]}"
	dur=$(sox --i -D "$src" | awk '{printf "%d", $1}')
	if [[ $dur -lt 2 ]]; then continue; fi
	start=$((RANDOM % (dur - 1)))
	sox "$src" -r 16000 -c 1 -b 16 "$SILENCE_OUT/silence_${i}.wav" \
		trim "$start" 1
	i=$((i + 1))
done

# --- unknown class ---------------------------------------------------------
# Sample from every top-level SC v2 word directory except _background_noise_
# (and skip any that collide with keyword classes already under out_root).
echo ">>> Sampling $N_UNKNOWN unknown clips into $UNKNOWN_OUT"
if [[ -z "${UNKNOWN_WORDS:-}" ]]; then
	mapfile -t word_dirs < <(find "$SC_CACHE" -maxdepth 1 -mindepth 1 -type d \
		! -name '_background_noise_')
else
	word_dirs=()
	for w in $UNKNOWN_WORDS; do word_dirs+=("$SC_CACHE/$w"); done
fi

# Skip words whose name already exists as an out_root/<word>/ class dir.
filtered=()
for d in "${word_dirs[@]}"; do
	w=$(basename "$d")
	if [[ -d "$OUT_ROOT/$w" && "$w" != "unknown" && "$w" != "silence" ]]; then
		echo "  skipping '$w' (collides with existing keyword class)"
		continue
	fi
	[[ -d "$d" ]] && filtered+=("$d")
done

# Enumerate all candidate wavs, shuf to N_UNKNOWN, sox-convert to 16k/mono/16b.
tmp_list=$(mktemp)
trap 'rm -f "$tmp_list"' EXIT
for d in "${filtered[@]}"; do
	find "$d" -name '*.wav' >> "$tmp_list"
done
if ! shuf --version >/dev/null 2>&1; then
	echo "shuf not on PATH — please install coreutils" >&2
	exit 1
fi
shuf -n "$N_UNKNOWN" "$tmp_list" | \
while read -r src; do
	base=$(basename "$src" .wav)
	# Prefix with parent dir name so we don't collide across words.
	parent=$(basename "$(dirname "$src")")
	sox "$src" -r 16000 -c 1 -b 16 \
		"$UNKNOWN_OUT/${parent}_${base}.wav" \
		2>/dev/null || true
done

if [[ "$GAIN_AUG" = "1" ]]; then
	echo ">>> Applying gain jitter to $UNKNOWN_OUT (peak=${GAIN_PEAK_DBFS} dBFS, sigma=${GAIN_SIGMA_DB} dB, headroom=${GAIN_HEADROOM_DB} dB)"
	apply_gain_jitter "$UNKNOWN_OUT"
fi

n_silence=$(find "$SILENCE_OUT" -name '*.wav' | wc -l)
n_unknown=$(find "$UNKNOWN_OUT" -name '*.wav' | wc -l)
echo ">>> Done: $n_silence silence / $n_unknown unknown WAVs under $OUT_ROOT"
