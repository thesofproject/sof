#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# Prepare the silence/ and unknown/ dataset classes for microWakeWord (MWW)
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
  GAIN_NORM         1 = peak-normalize each unknown/*.wav to GAIN_PEAK_DBFS
                    (silence/ is left alone so background noise stays
                    quiet). 0 = leave the clips at baseline level.
                    Default 1.
  GAIN_PEAK_DBFS    Peak-normalization target in dBFS (default -10).
EOF
	exit 1
fi

: "${SC_CACHE:=$HOME/.cache/speech_commands_v2}"
: "${N_SILENCE:=500}"
: "${N_UNKNOWN:=1500}"
: "${GAIN_NORM:=1}"
: "${GAIN_PEAK_DBFS:=-10}"

apply_peak_norm() {
	local dir="$1"
	local tmp
	tmp=$(mktemp --suffix=.wav)
	for wav in "$dir"/*.wav; do
		[ -f "$wav" ] || continue
		sox "$wav" "$tmp" gain -n "$GAIN_PEAK_DBFS"
		mv "$tmp" "$wav"
	done
	rm -f "$tmp"
}

if ! command -v sox >/dev/null; then
	echo "sox not on PATH — please install (e.g. apt install sox)" >&2
	exit 1
fi

if [[ ! -d "$SC_CACHE/_background_noise_" ]]; then
	echo ">>> Fetching Speech Commands v2 into $SC_CACHE (~2.4 GB, one-time)"
	mkdir -p "$SC_CACHE"
	url="https://download.tensorflow.org/data/speech_commands_v0.02.tar.gz"
	tarball="$SC_CACHE/speech_commands_v0.02.tar.gz"
	if [[ ! -f "$tarball" ]]; then
		wget -O "$tarball" "$url"
	fi
	tar -C "$SC_CACHE" -xzf "$tarball"
fi

SILENCE_OUT="$OUT_ROOT/silence"
UNKNOWN_OUT="$OUT_ROOT/unknown"
mkdir -p "$SILENCE_OUT" "$UNKNOWN_OUT"

echo ">>> Producing $N_SILENCE silence clips into $SILENCE_OUT"
rm -f "$SILENCE_OUT"/*.wav
bg_files=("$SC_CACHE/_background_noise_"/*.wav)
n_bg=${#bg_files[@]}
if [[ $n_bg -eq 0 ]]; then
	echo "no background noise files found under $SC_CACHE/_background_noise_" >&2
	exit 1
fi

i=0
while [[ $i -lt $N_SILENCE ]]; do
	bg="${bg_files[$((RANDOM % n_bg))]}"
	duration=$(soxi -D "$bg" | awk '{print int($1)}')
	if [[ $duration -le 1 ]]; then
		continue
	fi
	max_start=$((duration - 1))
	start=$((RANDOM % max_start))
	out="$SILENCE_OUT/silence_${i}.wav"
	sox "$bg" -r 16000 -c 1 -b 16 "$out" trim "$start" 1.0
	i=$((i + 1))
done

echo ">>> Sampling $N_UNKNOWN unknown words into $UNKNOWN_OUT"
rm -f "$UNKNOWN_OUT"/*.wav

if [[ -z "$UNKNOWN_WORDS" ]]; then
	word_dirs=()
	for d in "$SC_CACHE"/*/; do
		base=$(basename "$d")
		case "$base" in
			_background_noise_) ;;
			*) word_dirs+=("$d") ;;
		esac
	done
else
	word_dirs=()
	for w in $UNKNOWN_WORDS; do
		if [[ -d "$SC_CACHE/$w" ]]; then
			word_dirs+=("$SC_CACHE/$w")
		fi
	done
fi

n_words=${#word_dirs[@]}
if [[ $n_words -eq 0 ]]; then
	echo "no word directories found under $SC_CACHE" >&2
	exit 1
fi

all_wavs=()
for d in "${word_dirs[@]}"; do
	for f in "${d%/}"/*.wav; do
		[[ -f "$f" ]] && all_wavs+=("$f")
	done
done

n_all=${#all_wavs[@]}
if [[ $n_all -eq 0 ]]; then
	echo "no speech command WAVs found" >&2
	exit 1
fi

shuffled=($(printf "%s\n" "${all_wavs[@]}" | shuf -n "$N_UNKNOWN"))

i=0
for wav in "${shuffled[@]}"; do
	out="$UNKNOWN_OUT/unknown_${i}.wav"
	sox "$wav" -r 16000 -c 1 -b 16 "$out"
	i=$((i + 1))
done

if [[ "$GAIN_NORM" = "1" ]]; then
	echo ">>> Applying peak normalization (${GAIN_PEAK_DBFS} dBFS) to $UNKNOWN_OUT"
	apply_peak_norm "$UNKNOWN_OUT"
fi

echo ">>> Done. Dataset now contains:"
echo "    silence: $(find "$SILENCE_OUT" -name '*.wav' | wc -l) WAVs"
echo "    unknown: $(find "$UNKNOWN_OUT" -name '*.wav' | wc -l) WAVs"
