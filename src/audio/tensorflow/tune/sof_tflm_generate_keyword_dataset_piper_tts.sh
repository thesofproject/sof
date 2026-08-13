#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# Generate a synthetic keyword WAV dataset with a SINGLE-SPEAKER Piper TTS
# voice (piper-tts package). This is the language-agnostic companion to
# sof_tflm_generate_keyword_dataset.sh, which is hard-wired to the
# English multi-speaker piper-sample-generator (LibriTTS-R). Use this
# script for any non-English keyword whose Piper voice is single-speaker
# (fi_FI-harri, sv_SE-nst, de_DE-thorsten, es_ES-sharvard, and most other
# rhasspy/piper-voices entries).
#
# Because a single-speaker VITS has no speaker-embedding axis, we recover
# diversity in two other ways:
#   1. Per-utterance random draw of Piper's --noise-scale / --noise-w
#      (prosody stochasticity) plus a cycle over several --length-scale
#      values (speaking rate).
#   2. Post-synthesis sox pitch (cents) + tempo perturbation copies, which
#      approximates gender / fundamental-frequency variation across
#      unrelated speakers.
#
# The output layout matches sof_tflm_generate_keyword_dataset.sh:
#
#   <out_root>/<label>/*.wav        <- final training set
#   <out_root>/_raw/<label>/*.wav   <- staging (Piper TTS output)
#   <out_root>/_perturbed/<label>/*.wav <- staging (after sox)
#
# The underscore-prefixed dirs are skipped by sof_mfcc_extract_features.sh
# so they can stay next to the final set.
#
# Silence + unknown classes are still produced by
# sof_tflm_prepare_silence_unknown.sh from Speech Commands v2 (English
# clips are fine as negative examples even when the positive keyword is
# in another language).
#
# Requirements:
#   - piper-tts installed in an activated venv (or $PIPER_TTS_VENV set to
#     a venv root and this script activates it for you).
#   - A Piper voice: an .onnx file plus its .onnx.json config sibling.
#     Download once from rhasspy/piper-voices on HuggingFace, e.g.:
#       mkdir -p ~/git/piper-voices/fi_FI
#       cd ~/git/piper-voices/fi_FI
#       BASE=https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0
#       wget "$BASE/fi/fi_FI/harri/medium/fi_FI-harri-medium.onnx"
#       wget "$BASE/fi/fi_FI/harri/medium/fi_FI-harri-medium.onnx.json"
#   - sox on PATH.
#
# Optional: if PIPER_REPO points at a piper-sample-generator git clone,
# its augment module runs after synthesis to add room-impulse-response
# convolution and volume jitter (same augmentation the English pipeline
# uses). If PIPER_REPO is unset the pipeline falls back to a plain
# sox resample to 16 kHz mono and relies on the built-in gain jitter for
# level diversity.
#
# Env knobs (all optional):
#   PIPER_TTS_VENV     Path to a piper-tts venv to auto-activate. If unset
#                      (or bin/activate is missing), the script assumes
#                      the caller has already activated a venv or that
#                      piper-tts is on the ambient PATH.
#   PIPER_VOICE        Default .onnx voice path used when --voice is not
#                      given (or given fewer times than --keyword).
#   PIPER_REPO         Optional piper-sample-generator git clone; enables
#                      the IR-convolution augment step.
#   MAX_SAMPLES        Target total positive clips per keyword (default
#                      1000). Split between Piper synthesis calls and
#                      sox perturbation copies.
#   LENGTH_SCALES      Space-separated Piper --length-scale values to
#                      cycle through (default "0.85 0.95 1.00 1.10 1.20").
#   NOISE_SCALE_MIN    Piper --noise-scale lower bound (default 0.60).
#   NOISE_SCALE_MAX    Piper --noise-scale upper bound (default 0.68).
#   NOISE_W_MIN        Piper --noise-w lower bound (default 0.75).
#   NOISE_W_MAX        Piper --noise-w upper bound (default 0.85).
#                      These narrow ranges are tuned for single-speaker
#                      Piper voices, which tend to degrade audibly at the
#                      extremes of the wider ranges the multi-speaker
#                      LibriTTS-R generator can tolerate. Widen only if
#                      your voice is known to render cleanly there.
#   PERTURB_PER_UTT    sox pitch+tempo copies per synthesized utterance
#                      (default 3). Use 0 to skip perturbation entirely.
#   PITCH_CENTS_MAX    Absolute max pitch shift in cents (default 300;
#                      +/- 3 semitones covers most male<->female F0
#                      distance without introducing obvious artifacts).
#   TEMPO_MIN          sox `tempo -s` factor lower bound (default 0.9).
#   TEMPO_MAX          sox `tempo -s` factor upper bound (default 1.15).
#   GAIN_AUG           1 = rewrite each final WAV in place with a peak-
#                      normalized target and a per-file Gaussian gain
#                      jitter, matching the English generator. 0 = leave
#                      unmodified (default 0).
#   GAIN_PEAK_DBFS     Peak-normalization target in dBFS (default -10).
#   GAIN_SIGMA_DB      Gaussian jitter sigma in dB (default 5).
#   GAIN_HEADROOM_DB   Headroom below full scale the augmented peak must
#                      respect (default 1).
#   IR_PROB            Probability that a given synthesized WAV gets a
#                      room impulse response convolved in (default 0.75).
#                      Set to 0 to skip IR augmentation entirely; the
#                      remaining samples stay dry.
#   IR_WET             Wet/dry mix used when IR fires: 0.0 = fully dry,
#                      1.0 = fully wet (default 0.35). The wet signal is
#                      peak-matched to the dry signal before mixing so
#                      changing IR_WET only trades intelligibility for
#                      reverb amount, not loudness.
#   IR_DIR             Directory of *.wav impulse responses to draw from.
#                      Default: a curated subset of piper-sample-generator's
#                      bundled pool that excludes the musical/effect
#                      impulses (Symphonic, Reverse Gate, Fat Bass, Blatty
#                      Plate, Derlon Sanctuary) which produced unrealistic
#                      cathedral-like reverb in earlier training runs.
#
# Required CLI:
#   --keyword TEXT   Phrase to synthesize (e.g. "tietokone"). Repeat once
#                    per positive class.
#   --label DIR      Output subdir name. If omitted, derived from keyword
#                    by lower-casing and replacing spaces with underscores.
#                    When repeated, count must match --keyword.
#   --voice PATH     Piper .onnx voice. If omitted, $PIPER_VOICE is used.
#                    When repeated, count must match --keyword; this lets
#                    a multi-keyword run mix voices per class (e.g. one
#                    Finnish keyword plus one Swedish keyword).

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
: "${GAIN_AUG:=0}"
: "${GAIN_PEAK_DBFS:=-10}"
: "${GAIN_SIGMA_DB:=5}"
: "${GAIN_HEADROOM_DB:=1}"
: "${IR_PROB:=0.75}"
: "${IR_WET:=0.35}"
: "${IR_DIR:=}"

export LENGTH_SCALES NOISE_SCALE_MIN NOISE_SCALE_MAX NOISE_W_MIN NOISE_W_MAX

# Draw one N(0, sigma) sample in dB via awk Box-Muller, capped so the
# resulting peak (GAIN_PEAK_DBFS + offset) stays <= -GAIN_HEADROOM_DB.
# Kept identical to sof_tflm_generate_keyword_dataset.sh so the two
# variants produce statistically comparable level distributions.
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

if [[ -n "$PIPER_TTS_VENV" ]]; then
	if [[ -f "$PIPER_TTS_VENV/bin/activate" ]]; then
		# shellcheck disable=SC1091
		source "$PIPER_TTS_VENV/bin/activate"
	else
		echo ">>> PIPER_TTS_VENV=$PIPER_TTS_VENV has no bin/activate; assuming piper-tts is already on \$PATH" >&2
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

	# Split the target sample budget: BASE_UTTS Piper calls, each fanned
	# out into PERTURB_PER_UTT sox copies. Guarantee at least one full
	# sweep of LENGTH_SCALES so the rate axis is exercised.
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

# piper-tts changed its synthesize signature across versions:
#   >=1.3  voice.synthesize_wav(text, wav_file, syn_config=SynthesisConfig(...))
#          (SynthesisConfig field is either `noise_w` or `noise_w_scale`)
#   older  voice.synthesize(text, wav_file, length_scale=, noise_scale=, noise_w=)
# Probe once and lock in whichever shape works.
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

	# sox pitch/tempo perturbation. Bash's $RANDOM (0..32767) drives both
	# the pitch cents draw (integer) and the tempo factor (float via awk).
	# tempo -s preserves pitch, so pitch and tempo are decoupled the same
	# way real speaker variation is.
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
				# `gain -h` auto-attenuates by whatever headroom the
				# following pitch+tempo effects need so sox does not
				# emit dither/rate clip warnings; apply_gain_jitter
				# renormalizes to GAIN_PEAK_DBFS afterward.
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
		echo ">>> IR augmentation via audiomentations -> $FINAL_DIR (IR_PROB=$IR_PROB)"
		python3 - "$SRC_DIR" "$FINAL_DIR" <<'PYEOF'
import audioop
import glob
import os
import random
import sys
import wave
from pathlib import Path

import numpy as np
from audiomentations import ApplyImpulseResponse

src, dst = sys.argv[1], sys.argv[2]
Path(dst).mkdir(parents=True, exist_ok=True)
target_sr = 16000
ir_prob = float(os.environ.get("IR_PROB", "0.75"))
ir_wet = float(os.environ.get("IR_WET", "0.35"))
ir_wet = max(0.0, min(1.0, ir_wet))

# Curated whitelist: keep only the small live-room impulse; drop the
# musical/effect impulses (cathedral, plate, reverse gate, ...) and the
# large "Concrete Room" tail that was masking short keywords like
# "hi intel" ("hi needel").
KEEP = ("Accoustic2_Impulse.wav",)

ir_dir = os.environ.get("IR_DIR", "").strip()
if not ir_dir:
	default_dir = os.environ.get("DEFAULT_IR_DIR", "").strip()
	impulses = [os.path.join(default_dir, n) for n in KEEP
	            if default_dir and os.path.isfile(os.path.join(default_dir, n))]
else:
	impulses = sorted(glob.glob(os.path.join(ir_dir, "*.wav")))

rng = random.Random()

if impulses and ir_prob > 0.0:
	# ApplyImpulseResponse fires unconditionally (p=1.0); we gate it
	# ourselves so the wet/dry blend has a clean handle on which samples
	# were augmented. leave_length_unchanged=False keeps the reverb tail.
	ir_aug = ApplyImpulseResponse(impulses, p=1.0,
	                              leave_length_unchanged=False)
	names = ", ".join(os.path.basename(p) for p in impulses)
	print(f"    IR pool ({len(impulses)}): {names}", file=sys.stderr)
	print(f"    per-sample IR probability: {ir_prob:.2f}  wet mix: {ir_wet:.2f}",
	      file=sys.stderr)
else:
	ir_aug = None
	print("    IR augmentation disabled (empty pool or IR_PROB=0)",
	      file=sys.stderr)

for f in sorted(glob.glob(os.path.join(src, "*.wav"))):
	with wave.open(f, "rb") as wi:
		sr = wi.getframerate()
		nch = wi.getnchannels()
		sw = wi.getsampwidth()
		raw = wi.readframes(wi.getnframes())
	if sw != 2 or nch != 1:
		print(f"    skip non-16bit-mono: {f}", file=sys.stderr)
		continue
	x = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32767.0
	if ir_aug is not None and rng.random() < ir_prob:
		x_wet = ir_aug(x, sample_rate=sr)
		# Peak-match wet to dry so IR_WET only trades reverb amount,
		# not overall loudness (raw IRs have arbitrary gain).
		wet_peak = float(np.max(np.abs(x_wet))) if x_wet.size else 0.0
		dry_peak = float(np.max(np.abs(x))) if x.size else 0.0
		if wet_peak > 1e-9 and dry_peak > 1e-9:
			x_wet = x_wet * (dry_peak / wet_peak)
		pad = x_wet.size - x.size
		x_dry = np.concatenate([x, np.zeros(pad, dtype=x.dtype)]) if pad > 0 else x
		x = (1.0 - ir_wet) * x_dry + ir_wet * x_wet
	x16_bytes = np.clip(x * 32767.0, -32768, 32767).astype(np.int16).tobytes()
	if sr != target_sr:
		x16_bytes, _ = audioop.ratecv(x16_bytes, 2, 1, sr, target_sr, None)
	out = os.path.join(dst, os.path.basename(f))
	with wave.open(out, "wb") as wo:
		wo.setframerate(target_sr)
		wo.setsampwidth(2)
		wo.setnchannels(1)
		wo.writeframes(x16_bytes)
PYEOF
	else
		echo ">>> Resampling to 16 kHz mono (no IR augmentation) -> $FINAL_DIR"
		for f in "$SRC_DIR"/*.wav; do
			[ -f "$f" ] || continue
			sox "$f" -r 16000 -c 1 -b 16 "$FINAL_DIR/$(basename "$f")"
		done
	fi

	if [[ "$GAIN_AUG" = "1" ]]; then
		echo ">>> Applying gain jitter to $FINAL_DIR (peak=${GAIN_PEAK_DBFS} dBFS, sigma=${GAIN_SIGMA_DB} dB, headroom=${GAIN_HEADROOM_DB} dB)"
		apply_gain_jitter "$FINAL_DIR"
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
echo ">>> Next: sof_tflm_train_pipeline.sh$KW_ARGS $OUT_ROOT <feat_root> <out_dir>"
