#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# Generate a synthetic keyword WAV dataset for microWakeWord (MWW) with
# piper-sample-generator (multi-speaker LibriTTS-R), augment it (impulse-response
# convolution, volume jitter, resample to 16 kHz), and drop the result into
# the layout expected by sof_mfcc_extract_features.sh:
#
#   <out_root>/<label>/*.wav
#
# Companion `silence` and `unknown` classes are NOT produced here — those are
# dataset-wide and best sourced from Speech Commands v2 (see
# sof_mww_prepare_silence_unknown.sh).
#
# Requirements:
#   - piper-sample-generator installed in an activated venv (or $PIPER_VENV set).
#   - LibriTTS-R generator checkpoint at $PIPER_MODEL. Fetch once with:
#       wget -O <models>/en_US-libritts_r-medium.pt \
#         https://github.com/rhasspy/piper-sample-generator/releases/download/v2.0.0/en_US-libritts_r-medium.pt
#   - sox on PATH.
#
# Env knobs (all optional):
#   PIPER_VENV      Path to a Piper venv to auto-activate.
#   PIPER_MODEL     Path to the LibriTTS-R .pt checkpoint.
#   PIPER_REPO      Path to a piper-sample-generator git clone root.
#   MAX_SAMPLES     Positive clips per speaking-rate loop (default 1000).
#   MAX_SPEAKERS    Cap on speaker-embedding index (default 200).
#   SLERP_WEIGHTS   Speaker blending weights passed to generator (default "0.0").
#   GAIN_NORM          1 = peak-normalize each final WAV to GAIN_PEAK_DBFS (default 1).
#                   0 = leave baseline (default 1).
#   GAIN_PEAK_DBFS  Peak-normalization target in dBFS (default -10).
#   GAIN_SIGMA_DB   Gaussian jitter sigma in dB (default 5).
#   GAIN_HEADROOM_DB Headroom below full scale (default 1).
#   NOISE_AUG       1 = mix background noise onto positives, 0 = disable (default 1).
#   NOISE_PROB      Probability of mixing noise into a clip (default 0.9).
#   NOISE_SNR_MIN_DB Min SNR in dB (default 0).
#   NOISE_SNR_MAX_DB Max SNR in dB (default 25).
#   NOISE_DIR       Directory of *.wav background noise files (default:
#                   $SC_CACHE/_background_noise_ or ~/.cache/speech_commands_v2/_background_noise_).
#   NOISE_STATIONARY_BIAS Probability of picking from the stationary-noise subset
#                   (white/pink/exercise_bike/running_tap) when it exists (default 0.7).

set -e

KEYWORDS=()
LABELS=()
OUT_ROOT=""

usage() {
	cat >&2 <<EOF
Usage: $0 --keyword "<text>" [--keyword "<text>" ...] \\
          [--label <dir> ...] [--model <path.pt>] <out_root>

  --keyword TEXT   Spoken phrase to synthesize (repeatable; at least one).
  --label DIR      Output subdir name under <out_root>. Repeat once per
                   --keyword to override the default naming. If omitted,
                   each label is derived from its keyword by lower-casing
                   and replacing spaces with underscores.
  --model PATH     Path to PyTorch .pt checkpoint (default: \$PIPER_MODEL).
  --venv  PATH     Path to piper virtual environment (default: \$PIPER_VENV).
  out_root         Dataset root; <label>/*.wav is produced under it for each
                   keyword, ready to feed to
                   sof_mfcc_extract_features.sh <out_root> <feat_root>.

Optional env: PIPER_VENV, PIPER_MODEL, PIPER_REPO, MAX_SAMPLES, MAX_SPEAKERS.
EOF
	exit 1
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		-k|--keyword)
			[[ $# -ge 2 ]] || usage
			KEYWORDS+=("$2"); shift 2 ;;
		-l|--label)
			[[ $# -ge 2 ]] || usage
			LABELS+=("$2"); shift 2 ;;
		-m|--model)
			[[ $# -ge 2 ]] || usage
			PIPER_MODEL="$2"; shift 2 ;;
		--venv)
			[[ $# -ge 2 ]] || usage
			PIPER_VENV="$2"; shift 2 ;;
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

if [[ -z "$OUT_ROOT" || ${#KEYWORDS[@]} -eq 0 ]]; then
	usage
fi

if [[ ${#LABELS[@]} -gt 0 && ${#LABELS[@]} -ne ${#KEYWORDS[@]} ]]; then
	echo "--label given ${#LABELS[@]} time(s) but --keyword given ${#KEYWORDS[@]} time(s); counts must match" >&2
	exit 1
fi

if [[ ${#LABELS[@]} -eq 0 ]]; then
	for kw in "${KEYWORDS[@]}"; do
		LABELS+=("$(echo "$kw" | tr ' A-Z' '_a-z')")
	done
fi

: "${MAX_SAMPLES:=1000}"
: "${MAX_SPEAKERS:=200}"
: "${SLERP_WEIGHTS:=0.0}"
: "${GAIN_NORM:=1}"
: "${GAIN_PEAK_DBFS:=-10}"
: "${NOISE_AUG:=1}"
: "${NOISE_PROB:=0.9}"
: "${NOISE_SNR_MIN_DB:=0.0}"
: "${NOISE_SNR_MAX_DB:=25.0}"
: "${NOISE_DIR:=}"
: "${NOISE_STATIONARY_BIAS:=0.7}"
: "${SC_CACHE:=$HOME/.cache/speech_commands_v2}"

if [[ -z "$NOISE_DIR" && -d "$SC_CACHE/_background_noise_" ]]; then
	NOISE_DIR="$SC_CACHE/_background_noise_"
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

if [[ -n "$PIPER_VENV" ]]; then
	if [[ -f "$PIPER_VENV/bin/activate" ]]; then
		# shellcheck disable=SC1091
		source "$PIPER_VENV/bin/activate"
	else
		echo ">>> PIPER_VENV=$PIPER_VENV has no bin/activate; assuming environment is active" >&2
	fi
fi

if ! command -v python3 >/dev/null; then
	echo "python3 not on PATH" >&2
	exit 1
fi

if ! command -v sox >/dev/null; then
	echo "sox not on PATH" >&2
	exit 1
fi

if [[ -z "$PIPER_MODEL" ]]; then
	for cand in \
		"$HOME/git/piper-sample-generator/models/en_US-libritts_r-medium.pt" \
		"$HOME/.local/share/piper-sample-generator/en_US-libritts_r-medium.pt"
	do
		if [[ -f "$cand" ]]; then
			PIPER_MODEL="$cand"
			break
		fi
	done
fi

if [[ -z "$PIPER_MODEL" || ! -f "$PIPER_MODEL" ]]; then
	cat >&2 <<EOF
Piper generator checkpoint not found: ${PIPER_MODEL:-<unset>}
Fetch it once with:
  mkdir -p \$HOME/git/piper-sample-generator/models
  wget -O \$HOME/git/piper-sample-generator/models/en_US-libritts_r-medium.pt \\
    https://github.com/rhasspy/piper-sample-generator/releases/download/v2.0.0/en_US-libritts_r-medium.pt
or point \$PIPER_MODEL at the downloaded .pt file.
EOF
	exit 1
fi

if [[ -z "$PIPER_REPO" && "$PIPER_MODEL" == */models/*.pt ]]; then
	candidate=$(cd "$(dirname "$PIPER_MODEL")/.." && pwd)
	if [[ -d "$candidate/piper_train" ]]; then
		PIPER_REPO="$candidate"
	fi
fi

if [[ -n "$PIPER_REPO" ]]; then
	if ! python3 -c "import piper_train" 2>/dev/null; then
		export PYTHONPATH="$PIPER_REPO${PYTHONPATH:+:$PYTHONPATH}"
		echo ">>> Added $PIPER_REPO to PYTHONPATH for piper_train"
	fi
fi

if ! python3 -c "import piper_sample_generator" 2>/dev/null; then
	echo "piper_sample_generator not importable — activate the Piper venv first" >&2
	echo "(or pass --venv /path/to/venv or set PIPER_VENV=/path/to/venv)" >&2
	exit 1
fi

if ! python3 -c "import piper_train" 2>/dev/null; then
	cat >&2 <<EOF
piper_train not importable. Upstream ships it in the git repo but does not
pip-install it. Either:
  - set PIPER_REPO=/path/to/piper-sample-generator (git clone root), or
  - run this script with the repo root on PYTHONPATH.
EOF
	exit 1
fi

RESULTS=()
for i in "${!KEYWORDS[@]}"; do
	KEYWORD="${KEYWORDS[$i]}"
	LABEL="${LABELS[$i]}"
	RAW_DIR="$OUT_ROOT/_raw/$LABEL"
	FINAL_DIR="$OUT_ROOT/$LABEL"
	mkdir -p "$RAW_DIR" "$FINAL_DIR"

	echo ">>> [$((i + 1))/${#KEYWORDS[@]}] Generating '$KEYWORD' into $RAW_DIR"
	echo "    model=$PIPER_MODEL"
	echo "    max_samples=$MAX_SAMPLES per rate loop, max_speakers=$MAX_SPEAKERS"

	# Loop over speaking rates for prosody variety.
	for scale in 0.9 1.0 1.1; do
		echo ">>> length_scale=$scale"
		python3 -m piper_sample_generator "$KEYWORD" \
			--model "$PIPER_MODEL" \
			--max-samples "$MAX_SAMPLES" \
			--max-speakers "$MAX_SPEAKERS" \
			--slerp-weights $SLERP_WEIGHTS \
			--length-scales "$scale" \
			--output-dir "$RAW_DIR"
		tag="s$(echo "$scale" | tr -d .)"
		for f in "$RAW_DIR"/*.wav; do
			base=$(basename "$f" .wav)
			case "$base" in
				s*_*) continue ;;
			esac
			mv "$f" "$RAW_DIR/${tag}_${base}.wav"
		done
	done

	echo ">>> Augmenting into $FINAL_DIR (16 kHz resampling, Accoustic2 RIR, additive noise NOISE_PROB=$NOISE_PROB SNR=[$NOISE_SNR_MIN_DB,$NOISE_SNR_MAX_DB] dB)"
	export IR_AUG IR_PROB IR_WET IR_DIR
	export NOISE_AUG NOISE_PROB NOISE_SNR_MIN_DB NOISE_SNR_MAX_DB NOISE_DIR NOISE_STATIONARY_BIAS
	python3 - "$RAW_DIR" "$FINAL_DIR" <<'PYEOF'
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
noise_prob = float(os.environ.get("NOISE_PROB", "0.9"))
noise_snr_min = float(os.environ.get("NOISE_SNR_MIN_DB", "0.0"))
noise_snr_max = float(os.environ.get("NOISE_SNR_MAX_DB", "25.0"))
noise_dir = os.environ.get("NOISE_DIR", "").strip()
noise_stationary_bias = float(os.environ.get("NOISE_STATIONARY_BIAS", "0.7"))

# Broadband, near-stationary noise files in Google Speech Commands v2 _background_noise_.
# The runtime microphone floor is dominated by stationary broadband hiss, so we bias
# the mix toward these to match observed capture conditions.
STATIONARY_NOISE_NAMES = {
    "white_noise.wav", "pink_noise.wav",
    "exercise_bike.wav", "running_tap.wav",
}

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
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        b4 = np.pad(b, ((0,0),(1,0)), mode='constant')
        samples = (b4.view(np.int32).squeeze() >> 8).astype(np.float32) / 8388608.0
    elif sw == 4:
        samples = np.frombuffer(raw, dtype=np.int32).astype(np.float32) / 2147483648.0
    else:
        samples = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
    if nch > 1:
        samples = samples.reshape(-1, nch).mean(axis=1)

    if sr != target_sr:
        if has_scipy:
            g = gcd(target_sr, sr)
            samples = resample_poly(samples, target_sr // g, sr // g)
        else:
            orig_len = len(samples)
            new_len = int(orig_len * target_sr / sr)
            samples = np.interp(np.linspace(0, orig_len, new_len), np.arange(orig_len), samples)
    return samples

REALISTIC_IR_NAMES = {"Accoustic2_Impulse.wav"}

ir_files = []
if ir_aug_enabled and ir_dir and os.path.isdir(ir_dir):
    all_irs = sorted(glob.glob(os.path.join(ir_dir, "*.wav")))
    filtered = [f for f in all_irs if os.path.basename(f) in REALISTIC_IR_NAMES]
    ir_files = filtered if filtered else all_irs

if ir_files:
    names = ", ".join(os.path.basename(p) for p in ir_files)
    print(f"    Loaded {len(ir_files)} realistic RIR impulses ({names})", file=sys.stderr)
else:
    print("    RIR augmentation disabled or no impulse files found", file=sys.stderr)

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

noise_files = []
if noise_aug_enabled and noise_dir and os.path.isdir(noise_dir):
    noise_files = sorted(glob.glob(os.path.join(noise_dir, "*.wav")))

noise_cache = []
stationary_indices = []
for idx, n_p in enumerate(noise_files):
    n_samp = read_wav_float(n_p, target_sr=16000)
    if n_samp.size > 0:
        noise_cache.append(n_samp)
        if os.path.basename(n_p) in STATIONARY_NOISE_NAMES:
            stationary_indices.append(len(noise_cache) - 1)

if noise_cache:
    print(
        f"    Loaded {len(noise_cache)} noise files from {noise_dir} "
        f"({len(stationary_indices)} stationary, bias={noise_stationary_bias})",
        file=sys.stderr,
    )
else:
    print("    Noise augmentation disabled or no noise files found", file=sys.stderr)

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

def pick_noise_index():
    if stationary_indices and rng.random() < noise_stationary_bias:
        return rng.choice(stationary_indices)
    return rng.randrange(len(noise_cache))

for f in sorted(glob.glob(os.path.join(src_dir, "*.wav"))):
    x = read_wav_float(f, target_sr=16000)
    if x.size == 0:
        continue

    if ir_cache and rng.random() < ir_prob:
        h = rng.choice(ir_cache)
        if has_scipy:
            wet = fftconvolve(x, h, mode="full")
        else:
            wet = np.convolve(x, h, mode="full")

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

    if noise_cache and rng.random() < noise_prob:
        n_audio = noise_cache[pick_noise_index()]
        if n_audio.size >= x.size:
            start = np_rng.integers(0, n_audio.size - x.size + 1)
            n_slice = n_audio[start : start + x.size]
        else:
            reps = (x.size // n_audio.size) + 1
            n_slice = np.tile(n_audio, reps)[:x.size]

        speech_rms = float(np.sqrt(np.mean(x ** 2)))
        noise_rms = float(np.sqrt(np.mean(n_slice ** 2)))
        if speech_rms > 1e-5 and noise_rms > 1e-5:
            snr_db = np_rng.uniform(noise_snr_min, noise_snr_max)
            target_noise_rms = speech_rms * (10.0 ** (-snr_db / 20.0))
            x = x + (target_noise_rms / noise_rms) * n_slice

    out_name = os.path.basename(f)
    write_wav_int16(os.path.join(dst_dir, out_name), x, sr=16000)

PYEOF

	if [[ "$GAIN_NORM" = "1" ]]; then
		echo ">>> Applying peak normalization (${GAIN_PEAK_DBFS} dBFS) to $FINAL_DIR"
		apply_peak_norm "$FINAL_DIR"
	fi

	n_raw=$(find "$RAW_DIR" -name '*.wav' | wc -l)
	n_final=$(find "$FINAL_DIR" -name '*.wav' | wc -l)
	echo ">>> [$LABEL] $n_raw raw / $n_final augmented WAVs in $FINAL_DIR"
	RESULTS+=("$LABEL: $n_raw raw / $n_final augmented")
done

echo ">>> Done:"
for line in "${RESULTS[@]}"; do
	echo "    $line"
done
KW_ARGS=""
for lbl in "${LABELS[@]}"; do
	KW_ARGS+=" --keyword $lbl"
done
echo ">>> Next: sof_mww_train_pipeline.sh$KW_ARGS $OUT_ROOT <feat_root> <out_dir>"
