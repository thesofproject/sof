#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# Prepare and augment a positive keyword WAV dataset for microWakeWord (MWW)
# from a directory of recorded real speech WAV samples (e.g. collected microphone
# audio).
#
# Combines:
#   1. Conversion/resampling of clean real speech samples to 16 kHz 16-bit mono.
#   2. Retention of clean unperturbed samples in the dataset.
#   3. Sox pitch & tempo perturbation (prosody/speaker expansion).
#   4. Room Impulse Response (RIR) reverberation convolution.
#   5. Additive background noise mixing across a range of SNR levels (10-30 dB).
#   6. Gaussian level / gain jitter across volume levels (-12 dB to +6 dB / peak target).
#
# Output layout (ready for sof_mfcc_extract_features.sh / sof_mww_train_pipeline.sh):
#   <out_root>/<label>/*.wav       augmented and clean 16 kHz 16-bit mono clips
#   <out_root>/_raw/<label>/*.wav  clean resampled baseline clips
#
# Requirements:
#   - sox on PATH
#   - python3 with numpy and scipy (or audiomentations)
#
# Env knobs (all optional):
#   MAX_SAMPLES        Target total number of output WAVs per keyword (default 1000).
#   PERTURB_PER_UTT    Number of sox pitch/tempo perturbed copies per utterance
#                      (default: auto-calculated from MAX_SAMPLES, or set explicitly).
#   PITCH_CENTS_MAX    Max pitch shift in cents (+/- 250 = 2.5 semitones, default 250).
#   TEMPO_MIN/MAX      Min/max tempo multiplier (default: 0.90 to 1.15).
#   IR_AUG             1 = enable RIR convolution, 0 = disable (default 1).
#   IR_PROB            Probability of applying RIR to an augmented clip (default 0.50).
#   IR_WET             Wet/dry mix for RIR convolution (default 0.20).
#   IR_DIR             Directory containing *.wav impulse responses.
#   NOISE_AUG          1 = enable additive noise mixing, 0 = disable (default 1).
#   NOISE_PROB         Probability of mixing background noise (default 0.60).
#   NOISE_SNR_MIN_DB   Min SNR in dB for noise mixing (default 12.0).
#   NOISE_SNR_MAX_DB   Max SNR in dB for noise mixing (default 30.0).
#   NOISE_DIR          Directory containing background noise WAVs (default:
#                      $SC_CACHE/_background_noise_ or ~/.cache/speech_commands_v2/_background_noise_).
#   GAIN_NORM          1 = peak-normalize all clean WAVs to GAIN_PEAK_DBFS, 0 = keep input level (default 1).
#   GAIN_PEAK_DBFS     Peak-normalization target in dBFS (default -10).

set -e

SRC_DIRS=()
LABELS=()
OUT_ROOT=""

usage() {
	cat >&2 <<EOF
Usage: $0 --src-dir <dir> [--label <name>] \\
          [--src-dir <dir2> [--label <name2>] ...] <out_root>
       or:
       $0 --keyword-dir [<label>:]<dir> [...] <out_root>

  --src-dir DIR           Directory containing source .wav recordings of the keyword.
  --label   NAME          Output subdirectory name under <out_root> (e.g. hi_intel).
                          Defaults to the basename of DIR or common file prefix.
  --keyword-dir [LBL:]DIR Combination argument specifying label and source dir.
  out_root                Dataset root; <label>/*.wav is produced under it,
                          ready for sof_mfcc_extract_features.sh / sof_mww_train_pipeline.sh.

Optional env:
  MAX_SAMPLES, PERTURB_PER_UTT, PITCH_CENTS_MAX, TEMPO_MIN/MAX,
  IR_AUG, IR_PROB, IR_WET, IR_DIR,
  NOISE_AUG, NOISE_PROB, NOISE_SNR_MIN_DB, NOISE_SNR_MAX_DB, NOISE_DIR,
  GAIN_AUG, GAIN_PEAK_DBFS, GAIN_SIGMA_DB, GAIN_HEADROOM_DB.
EOF
	exit 1
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		-s|--src-dir)
			[[ $# -ge 2 ]] || usage
			SRC_DIRS+=("$2"); shift 2 ;;
		-l|--label)
			[[ $# -ge 2 ]] || usage
			LABELS+=("$2"); shift 2 ;;
		-k|--keyword-dir)
			[[ $# -ge 2 ]] || usage
			arg="$2"
			if [[ "$arg" == *:* ]]; then
				lbl="${arg%%:*}"
				sdir="${arg#*:}"
			else
				sdir="$arg"
				lbl="$(basename "$sdir")"
			fi
			SRC_DIRS+=("$sdir")
			LABELS+=("$lbl")
			shift 2 ;;
		-h|--help)
			usage ;;
		--)
			shift; break ;;
		-*)
			echo "unknown option: $1" >&2; usage ;;
		*)
			if [[ -z "$OUT_ROOT" ]]; then
				OUT_ROOT="$1"; shift
			else
				echo "unexpected positional: $1" >&2; usage
			fi ;;
	esac
done

if [[ -z "$OUT_ROOT" || ${#SRC_DIRS[@]} -eq 0 ]]; then
	usage
fi

if [[ ${#LABELS[@]} -gt 0 && ${#LABELS[@]} -ne ${#SRC_DIRS[@]} ]]; then
	echo "--label given ${#LABELS[@]} time(s) but --src-dir given ${#SRC_DIRS[@]} time(s); counts must match" >&2
	exit 1
fi

if [[ ${#LABELS[@]} -eq 0 ]]; then
	for sdir in "${SRC_DIRS[@]}"; do
		base="$(basename "$(realpath "$sdir")")"
		if [[ "$base" == "audio" || "$base" == "wav" || "$base" == "wavs" ]]; then
			parent="$(basename "$(dirname "$(realpath "$sdir")")")"
			base="$parent"
		fi
		LABELS+=("$(echo "$base" | tr ' A-Z-' '_a-z_')")
	done
fi

: "${MAX_SAMPLES:=1000}"
: "${PITCH_CENTS_MAX:=250}"
: "${TEMPO_MIN:=0.90}"
: "${TEMPO_MAX:=1.15}"
: "${IR_AUG:=1}"
: "${IR_PROB:=0.50}"
: "${IR_WET:=0.20}"
: "${IR_DIR:=}"
: "${NOISE_AUG:=1}"
: "${NOISE_PROB:=0.60}"
: "${NOISE_SNR_MIN_DB:=12.0}"
: "${NOISE_SNR_MAX_DB:=30.0}"
: "${NOISE_DIR:=}"
: "${GAIN_NORM:=1}"
: "${GAIN_PEAK_DBFS:=-10}"
: "${SC_CACHE:=$HOME/.cache/speech_commands_v2}"

if ! command -v sox >/dev/null; then
	echo "sox not on PATH — please install sox (e.g. apt install sox)" >&2
	exit 1
fi

if ! command -v python3 >/dev/null; then
	echo "python3 not on PATH" >&2
	exit 1
fi

# Locate default Impulse Response directory if not explicitly provided
if [[ -z "$IR_DIR" ]]; then
	for cand in \
		"$HOME/git/piper-sample-generator/piper_sample_generator/impulses" \
		"$HOME/.local/share/piper-sample-generator/impulses"
	do
		if [[ -d "$cand" ]]; then
			IR_DIR="$cand"
			break
		fi
	done
fi

# Locate default Noise directory if not explicitly provided
if [[ -z "$NOISE_DIR" ]]; then
	if [[ -d "$SC_CACHE/_background_noise_" ]]; then
		NOISE_DIR="$SC_CACHE/_background_noise_"
	fi
fi

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

RESULTS=()

for i in "${!SRC_DIRS[@]}"; do
	SRC_DIR="${SRC_DIRS[$i]}"
	LABEL="${LABELS[$i]}"

	if [[ ! -d "$SRC_DIR" ]]; then
		echo "Error: Source directory does not exist: $SRC_DIR" >&2
		exit 1
	fi

	RAW_DIR="$OUT_ROOT/_raw/$LABEL"
	PERT_DIR="$OUT_ROOT/_perturbed/$LABEL"
	FINAL_DIR="$OUT_ROOT/$LABEL"
	mkdir -p "$RAW_DIR" "$FINAL_DIR"
	rm -f "$RAW_DIR"/*.wav "$FINAL_DIR"/*.wav

	# Find input wavs
	raw_src_wavs=()
	while IFS= read -r -d '' f; do
		raw_src_wavs+=("$f")
	done < <(find "$SRC_DIR" -maxdepth 2 \( -name '*.wav' -o -name '*.WAV' \) -print0 | sort -z)

	n_src=${#raw_src_wavs[@]}
	if [[ $n_src -eq 0 ]]; then
		echo "Error: No WAV files found in $SRC_DIR" >&2
		exit 1
	fi

	echo ">>> [$((i + 1))/${#SRC_DIRS[@]}] Processing $n_src real speech WAVs for '$LABEL' from $SRC_DIR"

	# 1. Convert/resample clean files to 16 kHz 16-bit mono
	for idx in "${!raw_src_wavs[@]}"; do
		f="${raw_src_wavs[$idx]}"
		base="$(basename "$f" .wav)"
		base="$(basename "$base" .WAV)"
		out_clean="$RAW_DIR/${base}.wav"
		sox "$f" -r 16000 -c 1 -b 16 "$out_clean"
		# Place clean baseline into final output dir
		cp "$out_clean" "$FINAL_DIR/${base}_clean.wav"
	done

	echo "    Wrote $n_src clean 16 kHz mono reference clips into $RAW_DIR and $FINAL_DIR"

	# Calculate required copies per utterance
	if [[ -n "$PERTURB_PER_UTT" ]]; then
		n_copies="$PERTURB_PER_UTT"
	else
		if (( n_src < MAX_SAMPLES )); then
			n_copies=$(( (MAX_SAMPLES - n_src + n_src - 1) / n_src ))
		else
			n_copies=5
		fi
	fi
	(( n_copies < 1 )) && n_copies=1

	# 2. Sox pitch and tempo perturbations
	mkdir -p "$PERT_DIR"
	rm -f "$PERT_DIR"/*.wav
	echo "    Generating $n_copies pitch/tempo perturbations per clean recording -> $PERT_DIR"

	pcount=0
	for raw in "$RAW_DIR"/*.wav; do
		[ -f "$raw" ] || continue
		base=$(basename "$raw" .wav)
		for k in $(seq 1 "$n_copies"); do
			cents=$(( (RANDOM % (2 * PITCH_CENTS_MAX + 1)) - PITCH_CENTS_MAX ))
			tempo=$(awk -v a="$TEMPO_MIN" -v b="$TEMPO_MAX" -v r="$RANDOM" 'BEGIN{
				srand(r); printf "%.3f", a + rand()*(b-a)
			}')
			sox "$raw" "$PERT_DIR/${base}_p${k}.wav" \
				gain -h \
				pitch "$cents" tempo -s "$tempo"
			pcount=$((pcount + 1))
		done
	done
	echo "    Wrote $pcount perturbed copies"

	# 3. Acoustic augmentations: RIR convolution & Additive Noise
	export IR_AUG IR_PROB IR_WET IR_DIR
	export NOISE_AUG NOISE_PROB NOISE_SNR_MIN_DB NOISE_SNR_MAX_DB NOISE_DIR

	echo "    Applying acoustic augmentations (RIR: prob=$IR_PROB, Noise: prob=$NOISE_PROB SNR=[$NOISE_SNR_MIN_DB,$NOISE_SNR_MAX_DB] dB)"

	python3 - "$PERT_DIR" "$FINAL_DIR" <<'PYEOF'
import glob
from math import gcd
import os
import random
import sys
import wave
from pathlib import Path
import numpy as np

src_dir, dst_dir = sys.argv[1], sys.argv[2]
Path(dst_dir).mkdir(parents=True, exist_ok=True)

ir_aug_enabled = os.environ.get("IR_AUG", "1") == "1"
ir_prob = float(os.environ.get("IR_PROB", "0.50"))
ir_wet = float(os.environ.get("IR_WET", "0.20"))
ir_dir = os.environ.get("IR_DIR", "").strip()

noise_aug_enabled = os.environ.get("NOISE_AUG", "1") == "1"
noise_prob = float(os.environ.get("NOISE_PROB", "0.60"))
noise_snr_min = float(os.environ.get("NOISE_SNR_MIN_DB", "12.0"))
noise_snr_max = float(os.environ.get("NOISE_SNR_MAX_DB", "30.0"))
noise_dir = os.environ.get("NOISE_DIR", "").strip()

# Try importing scipy for fast FFT convolution and high-quality resampling
try:
    from scipy.signal import fftconvolve, resample_poly
    has_scipy = True
except ImportError:
    has_scipy = False

def read_wav_float(path, target_sr=16000):
    with wave.open(path, "rb") as wi:
        sr = wi.getframerate()
        nch = wi.getnchannels()
        sw = wi.getsampwidth()
        nframes = wi.getnframes()
        raw = wi.readframes(nframes)
    if sw == 2:
        samples = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
    elif sw == 3:
        # 24-bit PCM
        b = np.frombuffer(raw, dtype=np.uint8)
        b3 = b.reshape(-1, 3)
        b4 = np.pad(b3, ((0,0),(1,0)), mode='constant')
        s32 = b4.view(np.int32).squeeze() >> 8
        samples = s32.astype(np.float32) / 8388608.0
    elif sw == 4:
        samples = np.frombuffer(raw, dtype=np.int32).astype(np.float32) / 2147483648.0
    else:
        samples = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
    if nch > 1:
        samples = samples.reshape(-1, nch).mean(axis=1)

    # Resample to target_sr if necessary
    if sr != target_sr:
        if has_scipy:
            g = gcd(target_sr, sr)
            samples = resample_poly(samples, target_sr // g, sr // g)
        else:
            orig_len = len(samples)
            new_len = int(orig_len * target_sr / sr)
            samples = np.interp(np.linspace(0, orig_len, new_len), np.arange(orig_len), samples)
    return samples

# Curate realistic room impulse responses (filter out huge cathedrals / FX like Derlon Sanctuary / Reverse Gate)
REALISTIC_IR_NAMES = {"Accoustic2_Impulse.wav", "Concrete Room.wav", "ir_bathroom1.wav", "Symphonic.wav"}

ir_files = []
if ir_aug_enabled and ir_dir and os.path.isdir(ir_dir):
    all_irs = sorted(glob.glob(os.path.join(ir_dir, "*.wav")))
    # If default impulse folder, filter to realistic room acoustics
    filtered = [f for f in all_irs if os.path.basename(f) in REALISTIC_IR_NAMES]
    ir_files = filtered if filtered else all_irs

if ir_files:
    names = ", ".join(os.path.basename(p) for p in ir_files)
    print(f"      Loaded {len(ir_files)} realistic RIR impulses ({names})", file=sys.stderr)
else:
    print("      RIR augmentation disabled or no impulse files found", file=sys.stderr)

# Load Noise Files
noise_files = []
if noise_aug_enabled and noise_dir and os.path.isdir(noise_dir):
    noise_files = sorted(glob.glob(os.path.join(noise_dir, "*.wav")))
if noise_files:
    print(f"      Loaded {len(noise_files)} noise files from {noise_dir}", file=sys.stderr)
else:
    print("      Noise augmentation disabled or no noise files found", file=sys.stderr)

# Pre-load and trim RIRs to realistic room duration (max 350 ms = 5600 samples at 16k)
ir_cache = []
max_ir_samples = int(0.35 * 16000)
for ir_p in ir_files:
    h = read_wav_float(ir_p, target_sr=16000)
    if h.size > 0:
        if h.size > max_ir_samples:
            h = h[:max_ir_samples]
            fade_len = int(0.10 * max_ir_samples)
            h[-fade_len:] *= np.linspace(1.0, 0.0, fade_len)
        ir_cache.append(h)

# Pre-load noise waveforms resampled to 16 kHz
noise_cache = []
for n_p in noise_files:
    n_samp = read_wav_float(n_p, target_sr=16000)
    if n_samp.size > 0:
        noise_cache.append(n_samp)

def write_wav_int16(path, samples, sr=16000):
    samples = np.clip(samples, -1.0, 1.0)
    int16_data = (samples * 32767.0).astype(np.int16).tobytes()
    with wave.open(path, "wb") as wo:
        wo.setframerate(sr)
        wo.setnchannels(1)
        wo.setsampwidth(2)
        wo.writeframes(int16_data)

rng = random.Random(42)
np_rng = np.random.default_rng(42)

perturbed_files = sorted(glob.glob(os.path.join(src_dir, "*.wav")))
for f in perturbed_files:
    x = read_wav_float(f, target_sr=16000)
    if x.size == 0:
        continue

    # A. Apply subtle room reverberation
    if ir_cache and rng.random() < ir_prob:
        h = rng.choice(ir_cache)
        if has_scipy:
            wet = fftconvolve(x, h, mode="full")
        else:
            wet = np.convolve(x, h, mode="full")

        # Cap reverb tail to original speech length + at most 200 ms
        max_out_len = x.size + int(0.20 * 16000)
        if wet.size > max_out_len:
            wet = wet[:max_out_len]

        dry_peak = np.max(np.abs(x)) if x.size else 0.0
        wet_peak = np.max(np.abs(wet)) if wet.size else 0.0
        if wet_peak > 1e-9 and dry_peak > 1e-9:
            wet = wet * (dry_peak / wet_peak)

        pad = wet.size - x.size
        dry_padded = np.pad(x, (0, pad)) if pad > 0 else x[:wet.size]
        x = (1.0 - ir_wet) * dry_padded + ir_wet * wet

    # B. Apply Additive Background Noise (sliced exactly to match speech duration)
    if noise_cache and rng.random() < noise_prob:
        n_audio = rng.choice(noise_cache)
        if n_audio.size >= x.size:
            start = np_rng.integers(0, n_audio.size - x.size + 1)
            n_slice = n_audio[start : start + x.size]
        elif n_audio.size > 0:
            reps = (x.size // n_audio.size) + 1
            n_slice = np.tile(n_audio, reps)[:x.size]
        else:
            n_slice = None

        if n_slice is not None:
            speech_rms = np.sqrt(np.mean(x ** 2))
            noise_rms = np.sqrt(np.mean(n_slice ** 2))
            if speech_rms > 1e-5 and noise_rms > 1e-5:
                snr_db = np_rng.uniform(noise_snr_min, noise_snr_max)
                target_noise_rms = speech_rms * (10.0 ** (-snr_db / 20.0))
                scale = target_noise_rms / noise_rms
                x = x + scale * n_slice

    out_name = os.path.basename(f)
    write_wav_int16(os.path.join(dst_dir, out_name), x, sr=16000)

PYEOF

	# 4. Peak Normalization
	if [[ "$GAIN_NORM" = "1" ]]; then
		echo "    Applying peak normalization (${GAIN_PEAK_DBFS} dBFS) to $FINAL_DIR"
		apply_peak_norm "$FINAL_DIR"
	fi

	n_clean=$(find "$RAW_DIR" -name '*.wav' | wc -l)
	n_final=$(find "$FINAL_DIR" -name '*.wav' | wc -l)
	echo ">>> [$LABEL] Generated $n_final total WAVs ($n_clean clean + $((n_final - n_clean)) augmented) in $FINAL_DIR"
	RESULTS+=("$LABEL: $n_clean clean / $n_final total WAVs")
done

echo ">>> Dataset preparation from recorded audio complete:"
for line in "${RESULTS[@]}"; do
	echo "    $line"
done
KW_ARGS=""
for lbl in "${LABELS[@]}"; do
	KW_ARGS+=" --keyword $lbl"
done
echo ">>> Next step: sof_mww_train_pipeline.sh$KW_ARGS $OUT_ROOT <feat_root> <out_dir>"
