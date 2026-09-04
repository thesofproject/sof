#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# Generate a synthetic keyword WAV dataset for microWakeWord (MWW) with a
# SINGLE-SPEAKER Piper TTS voice (piper-tts package). This is the
# language-agnostic companion to sof_mww_generate_keyword_dataset.sh, which is
# hard-wired to the English multi-speaker piper-sample-generator (LibriTTS-R).
# Use this script for any keyword whose Piper voice is single-speaker
# (e.g. fi_FI-harri, sv_SE-nst, de_DE-thorsten, en_US-lessac).
#
# Output layout:
#   <out_root>/<label>/*.wav     augmented 16 kHz 16-bit mono clips
#   <out_root>/_raw/<label>/*.wav dry unperturbed baseline
#
# Requirements:
#   - piper-tts installed in a venv (or on $PATH)
#   - sox on PATH
#   - audiomentations (optional, for impulse response convolution)
#
# Env knobs (all optional):
#   PIPER_TTS_VENV     Path to a venv containing piper-tts. Auto-activated if set.
#   PIPER_VOICE        Default Piper voice (.onnx) when --voice is not passed.
#   PIPER_REPO         Path to a piper-sample-generator clone (used to find its
#                      curated impulse response pool).
#   MAX_SAMPLES        Target output sample count per keyword (default 1000).
#   LENGTH_SCALES      Space-separated speaking rates for Piper synthesis
#                      (default: "0.85 0.95 1.00 1.10 1.20").
#   NOISE_SCALE_MIN/MAX Phoneme duration variability (default: 0.60 to 0.68).
#   NOISE_W_MIN/MAX    Speaking pitch variability (default: 0.75 to 0.85).
#   PERTURB_PER_UTT    Number of sox pitch/tempo perturbed copies per base
#                      utterance (default 3; set to 0 to disable).
#   PITCH_CENTS_MAX    Max pitch shift in cents (+/- 300 = 3 semitones).
#   TEMPO_MIN/MAX      Min/max tempo multiplier (default: 0.9 to 1.15).
#   GAIN_NORM          1 = peak-normalize each final WAV to GAIN_PEAK_DBFS (default 1).
#                      0 = leave at baseline slice level (default 0).
#   GAIN_PEAK_DBFS     Peak-normalization target in dBFS (default -10).
#   GAIN_SIGMA_DB      Gaussian jitter sigma in dB (default 5).
#   GAIN_HEADROOM_DB   Headroom below full-scale to prevent clipping (default 1).
#   IR_PROB            Probability of applying Room Impulse Response (default 0.75).
#   IR_WET             Wet/dry mix for IR convolution (default 0.35).
#   IR_DIR             Directory of *.wav impulse responses.

set -e

KEYWORDS=()
LABELS=()
VOICES=()
OUT_ROOT=""

usage() {
	cat >&2 <<EOF
Usage: $0 --keyword "<text>" [--keyword "<text>" ...] \\
          [--label <dir> ...] [--voice <path> ...] <out_root>

  --keyword TEXT   Phrase to synthesize (repeatable; at least one).
  --label  DIR     Output subdir under <out_root> (repeatable; must pair
                   1:1 with --keyword when used). Default: lower-case
                   keyword with spaces replaced by underscores.
  --voice  PATH    Piper .onnx voice file (repeatable; must pair 1:1
                   with --keyword when used). Default: \$PIPER_VOICE.
  out_root         Dataset root; <label>/*.wav is produced under it for
                   each keyword, ready to feed to
                   sof_mfcc_extract_features.sh <out_root> <feat_root>.

Optional env: PIPER_TTS_VENV, PIPER_VOICE, PIPER_REPO, MAX_SAMPLES,
              LENGTH_SCALES, NOISE_SCALE_MIN/MAX, NOISE_W_MIN/MAX,
              PERTURB_PER_UTT, PITCH_CENTS_MAX, TEMPO_MIN/MAX,
              GAIN_AUG, GAIN_PEAK_DBFS, GAIN_SIGMA_DB, GAIN_HEADROOM_DB,
              IR_PROB, IR_WET, IR_DIR.
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
		-v|--voice)
			[[ $# -ge 2 ]] || usage
			VOICES+=("$2"); shift 2 ;;
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

if [[ ${#VOICES[@]} -gt 0 && ${#VOICES[@]} -ne ${#KEYWORDS[@]} ]]; then
	echo "--voice given ${#VOICES[@]} time(s) but --keyword given ${#KEYWORDS[@]} time(s); counts must match" >&2
	exit 1
fi

if [[ ${#LABELS[@]} -eq 0 ]]; then
	for kw in "${KEYWORDS[@]}"; do
		LABELS+=("$(echo "$kw" | tr ' A-Z' '_a-z')")
	done
fi

: "${MAX_SAMPLES:=1000}"
: "${LENGTH_SCALES:=0.85 0.95 1.00 1.10 1.20}"
: "${NOISE_SCALE_MIN:=0.60}"
: "${NOISE_SCALE_MAX:=0.68}"
: "${NOISE_W_MIN:=0.75}"
: "${NOISE_W_MAX:=0.85}"
: "${PERTURB_PER_UTT:=3}"
: "${PITCH_CENTS_MAX:=300}"
: "${TEMPO_MIN:=0.9}"
: "${TEMPO_MAX:=1.15}"
: "${GAIN_NORM:=1}"
: "${GAIN_PEAK_DBFS:=-10}"
: "${IR_PROB:=0.75}"
: "${IR_WET:=0.35}"
: "${IR_DIR:=}"

export LENGTH_SCALES NOISE_SCALE_MIN NOISE_SCALE_MAX NOISE_W_MIN NOISE_W_MAX

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

if [[ -n "$PIPER_TTS_VENV" ]]; then
	if [[ -f "$PIPER_TTS_VENV/bin/activate" ]]; then
		# shellcheck disable=SC1091
		source "$PIPER_TTS_VENV/bin/activate"
	else
		echo ">>> PIPER_TTS_VENV=$PIPER_TTS_VENV has no bin/activate; assuming piper-tts is on PATH" >&2
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

if ! python3 -c "import piper" 2>/dev/null; then
	cat >&2 <<EOF
piper (piper-tts) not importable. Install once with:

    python3 -m venv \$HOME/venvs/piper-tts
    source \$HOME/venvs/piper-tts/bin/activate
    pip install --upgrade pip
    pip install piper-tts

Then rerun this script (or set PIPER_TTS_VENV=/path/to/venv).
EOF
	exit 1
fi

USE_IR_AUG=0
DEFAULT_IR_DIR=""
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

if [[ -n "$PIPER_REPO" ]]; then
	if ! python3 -c "import piper_sample_generator" 2>/dev/null; then
		export PYTHONPATH="$PIPER_REPO${PYTHONPATH:+:$PYTHONPATH}"
	fi
	DEFAULT_IR_DIR="$PIPER_REPO/piper_sample_generator/impulses"
fi
if awk -v p="$IR_PROB" 'BEGIN{exit !(p+0 > 0)}' && \
   { [[ -n "$IR_DIR" ]] || [[ -d "$DEFAULT_IR_DIR" ]]; }; then
	if python3 -c "import audiomentations" 2>/dev/null; then
		USE_IR_AUG=1
	else
		echo ">>> WARNING: audiomentations not importable; skipping IR augmentation (pip install audiomentations)" >&2
	fi
fi
export IR_PROB IR_WET IR_DIR
export DEFAULT_IR_DIR

RESULTS=()
for i in "${!KEYWORDS[@]}"; do
	KEYWORD="${KEYWORDS[$i]}"
	LABEL="${LABELS[$i]}"
	VOICE="${VOICES[$i]:-$PIPER_VOICE}"

	if [[ -z "$VOICE" ]]; then
		echo "no voice supplied for keyword '$KEYWORD' (set --voice or \$PIPER_VOICE)" >&2
		exit 1
	fi
	if [[ ! -f "$VOICE" ]]; then
		echo "Piper voice file not found: $VOICE" >&2
		exit 1
	fi
	if [[ ! -f "$VOICE.json" ]]; then
		echo "Piper voice config not found: $VOICE.json (must sit next to the .onnx)" >&2
		exit 1
	fi

	RAW_DIR="$OUT_ROOT/_raw/$LABEL"
	PERT_DIR="$OUT_ROOT/_perturbed/$LABEL"
	FINAL_DIR="$OUT_ROOT/$LABEL"
	mkdir -p "$RAW_DIR" "$FINAL_DIR"
	rm -f "$RAW_DIR"/*.wav "$FINAL_DIR"/*.wav
	if [[ "$PERTURB_PER_UTT" -gt 0 ]]; then
		mkdir -p "$PERT_DIR"
		rm -f "$PERT_DIR"/*.wav
	fi

	N_SCALES=$(echo "$LENGTH_SCALES" | wc -w)
	if [[ "$PERTURB_PER_UTT" -gt 0 ]]; then
		BASE_UTTS=$(( MAX_SAMPLES / PERTURB_PER_UTT ))
	else
		BASE_UTTS="$MAX_SAMPLES"
	fi
	if (( BASE_UTTS < N_SCALES )); then
		BASE_UTTS=$N_SCALES
	fi

	echo ">>> [$((i + 1))/${#KEYWORDS[@]}] Synthesizing '$KEYWORD' -> $RAW_DIR"
	echo "    voice=$VOICE"
	echo "    base_utts=$BASE_UTTS  perturb_per_utt=$PERTURB_PER_UTT  target=$MAX_SAMPLES"
	echo "    length_scales=[$LENGTH_SCALES]  noise_scale=[$NOISE_SCALE_MIN,$NOISE_SCALE_MAX]  noise_w=[$NOISE_W_MIN,$NOISE_W_MAX]"

	python3 - "$KEYWORD" "$VOICE" "$RAW_DIR" "$BASE_UTTS" <<'PYEOF'
import os
import random
import sys
import wave
from pathlib import Path

try:
	from piper import PiperVoice
except ImportError:
	from piper.voice import PiperVoice

text, voice_path, out_dir_s, n_utts_s = sys.argv[1:5]
out_dir = Path(out_dir_s)
n_utts = int(n_utts_s)

length_scales = [float(x) for x in os.environ["LENGTH_SCALES"].split()]
ns_min = float(os.environ["NOISE_SCALE_MIN"])
ns_max = float(os.environ["NOISE_SCALE_MAX"])
nw_min = float(os.environ["NOISE_W_MIN"])
nw_max = float(os.environ["NOISE_W_MAX"])

voice = PiperVoice.load(voice_path)
rng = random.Random()

synth = None
if hasattr(voice, "synthesize_wav"):
	try:
		from piper import SynthesisConfig
	except ImportError:
		from piper.config import SynthesisConfig

	nw_field = None
	for candidate in ("noise_w_scale", "noise_w"):
		try:
			SynthesisConfig(length_scale=1.0, noise_scale=0.6, **{candidate: 0.8})
			nw_field = candidate
			break
		except TypeError:
			continue
	if nw_field is None:
		raise RuntimeError(
			"SynthesisConfig has neither noise_w nor noise_w_scale; "
			"unsupported piper-tts version"
		)

	def synth(wf, ls, ns, nw):
		cfg = SynthesisConfig(length_scale=ls, noise_scale=ns, **{nw_field: nw})
		voice.synthesize_wav(text, wf, syn_config=cfg)
else:
	def synth(wf, ls, ns, nw):
		voice.synthesize(
			text, wf,
			length_scale=ls,
			noise_scale=ns,
			noise_w=nw,
		)

for i in range(n_utts):
	ls = length_scales[i % len(length_scales)]
	ns = rng.uniform(ns_min, ns_max)
	nw = rng.uniform(nw_min, nw_max)
	out = out_dir / f"{i:04d}.wav"
	with wave.open(str(out), "wb") as wf:
		synth(wf, ls, ns, nw)

print(f"    piper synthesized {n_utts} base utterances", file=sys.stderr)
PYEOF

	if [[ "$PERTURB_PER_UTT" -gt 0 ]]; then
		echo ">>> Perturbing $BASE_UTTS raw WAVs x $PERTURB_PER_UTT copies -> $PERT_DIR"
		pcount=0
		for raw in "$RAW_DIR"/*.wav; do
			[ -f "$raw" ] || continue
			base=$(basename "$raw" .wav)
			for k in $(seq 1 "$PERTURB_PER_UTT"); do
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
		echo "    wrote $pcount perturbed WAVs"
		SRC_DIR="$PERT_DIR"
	else
		SRC_DIR="$RAW_DIR"
	fi

	if [[ "$USE_IR_AUG" = "1" ]]; then
		echo ">>> IR augmentation (16 kHz curated RIR convolution, IR_PROB=$IR_PROB) -> $FINAL_DIR"
		export IR_PROB IR_WET IR_DIR DEFAULT_IR_DIR
		python3 - "$SRC_DIR" "$FINAL_DIR" <<'PYEOF'
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

ir_prob = float(os.environ.get("IR_PROB", "0.50"))
ir_wet = float(os.environ.get("IR_WET", "0.20"))
ir_dir = os.environ.get("IR_DIR", "").strip()
default_dir = os.environ.get("DEFAULT_IR_DIR", "").strip()

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

REALISTIC_IR_NAMES = {"Accoustic2_Impulse.wav", "Concrete Room.wav", "ir_bathroom1.wav", "Symphonic.wav"}

search_dir = ir_dir if (ir_dir and os.path.isdir(ir_dir)) else default_dir
ir_files = []
if search_dir and os.path.isdir(search_dir):
    all_irs = sorted(glob.glob(os.path.join(search_dir, "*.wav")))
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

def write_wav_int16(path, samples, sr=16000):
    samples = np.clip(samples, -1.0, 1.0)
    int16_data = (samples * 32767.0).astype(np.int16).tobytes()
    with wave.open(path, "wb") as wo:
        wo.setframerate(sr)
        wo.setnchannels(1)
        wo.setsampwidth(2)
        wo.writeframes(int16_data)

rng = random.Random(42)

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

    out_name = os.path.basename(f)
    write_wav_int16(os.path.join(dst_dir, out_name), x, sr=16000)

PYEOF
	else
		echo ">>> Resampling to 16 kHz mono (no IR augmentation) -> $FINAL_DIR"
		for f in "$SRC_DIR"/*.wav; do
			[ -f "$f" ] || continue
			sox "$f" -r 16000 -c 1 -b 16 "$FINAL_DIR/$(basename "$f")"
		done
	fi

	if [[ "$GAIN_NORM" = "1" ]]; then
		echo ">>> Applying peak normalization (${GAIN_PEAK_DBFS} dBFS) to $FINAL_DIR"
		apply_peak_norm "$FINAL_DIR"
	fi

	n_raw=$(find "$RAW_DIR" -name '*.wav' | wc -l)
	n_final=$(find "$FINAL_DIR" -name '*.wav' | wc -l)
	echo ">>> [$LABEL] $n_raw raw / $n_final final WAVs in $FINAL_DIR"
	RESULTS+=("$LABEL: $n_raw raw / $n_final final (voice=$(basename "$VOICE"))")
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
