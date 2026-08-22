#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# End-to-end SOF TFLM wake-word retraining pipeline:
#
#   1. Prepare silence/ + unknown/ WAVs (Speech Commands v2 fetch + slice).
#   2. Assume the caller has already run
#      sof_tflm_generate_keyword_dataset.sh (or an equivalent) to populate
#      <wav_root>/<keyword>/*.wav for every keyword class.
#   3. Emit mel40 features via sof_mfcc_extract_features.sh -> <feat_root>/.
#   4. Train a tiny_conv model, int8-quantize with a real-feature
#      representative dataset, and emit the drop-in
#      <name>_quantized_model_data.{cc,h} + <name>_labels.txt under <out_dir>.
#
# The labels are locked at silence(0), unknown(1), <keyword1>(2),
# <keyword2>(3), ... so the on-device tflmcly KPB trigger rule
# (max_idx >= 2) fires for any positive keyword class.
#
# Multiple keywords: pass --keyword repeatedly, e.g.
#
#   sof_tflm_train_pipeline.sh --keyword banana --keyword mango --keyword orange \
#       ~/wov/wavs ~/wov/feats ~/wov/model
#
# Prerequisites:
#   - Piper venv with piper-sample-generator (used earlier to synthesize the
#     keyword WAVs).
#   - tflm-train venv with tensorflow + numpy activated before running this
#     script (source <venv>/bin/activate).
#   - sof-testbench4 built; sof-hda-benchmark-mfccmel4032.tplg available.
#   - sox, xxd on PATH.
set -e

usage() {
	cat >&2 <<EOF
Usage: $0 --keyword <label> [--keyword <label> ...] [--name <base>] \\
          <wav_root> <feat_root> <out_dir>

  --keyword LBL   Positive class directory name (repeatable). Must match
                  the subdir created by sof_tflm_generate_keyword_dataset.sh
                  under <wav_root>. Order defines model output indices
                  starting at 2 (silence=0, unknown=1, keyword_i=i+2).
  --name BASE     Base name for the output model files and C symbol
                  (default: first keyword when single-keyword, otherwise
                  the keyword labels joined with '_').

  wav_root        Dataset root; must already contain <keyword>/*.wav for each
                  --keyword. silence/ and unknown/ are populated here.
  feat_root       Mel40 feature output root; <label>/*.raw is produced here.
  out_dir         Where to write <name>_quantized_model.tflite, matching
                  .cc/.h C-array files, and a labels.txt manifest.

Env:
  SKIP_PREP        If set, do not fetch/slice Speech Commands v2 again.
  SKIP_FEATURES    If set, do not re-run testbench feature extraction.
  EPOCHS, BATCH_SIZE, LR, WINDOW_HOP_STEP
                   Passed through to sof_tflm_train.py.
  GAIN_AUG_DB_MIN, GAIN_AUG_DB_MAX
                   Uniform per-example gain augmentation in audio dB,
                   applied to log10-mel features before clip. Defaults
                   inside sof_tflm_train.py give a 30 dB span
                   ([-25.0, +5.0] dB) centered at -10 dB peak.
  GAIN_AUG, GAIN_PEAK_DBFS, GAIN_SIGMA_DB, GAIN_HEADROOM_DB
                   Passed through to the WAV generators
                   (sof_tflm_generate_keyword_dataset.sh and
                   sof_tflm_prepare_silence_unknown.sh). Default is
                   GAIN_AUG=0 (disabled); GAIN_AUG=1 enables gain
                   jitter to keyword and unknown WAVs (peak -10 dBFS,
                   sigma 5 dB).
EOF
	exit 1
}

KEYWORDS=()
NAME=""
POSITIONAL=()

while [[ $# -gt 0 ]]; do
	case "$1" in
		-k|--keyword)
			[[ $# -ge 2 ]] || usage
			KEYWORDS+=("$2"); shift 2 ;;
		-n|--name)
			[[ $# -ge 2 ]] || usage
			NAME="$2"; shift 2 ;;
		-h|--help)
			usage ;;
		--)
			shift
			while [[ $# -gt 0 ]]; do POSITIONAL+=("$1"); shift; done ;;
		-*)
			echo "unknown option: $1" >&2; usage ;;
		*)
			POSITIONAL+=("$1"); shift ;;
	esac
done

if [[ ${#KEYWORDS[@]} -eq 0 || ${#POSITIONAL[@]} -ne 3 ]]; then
	usage
fi

WAV_ROOT="${POSITIONAL[0]}"
FEAT_ROOT="${POSITIONAL[1]}"
OUT_DIR="${POSITIONAL[2]}"

if [[ -z "$NAME" ]]; then
	if [[ ${#KEYWORDS[@]} -eq 1 ]]; then
		NAME="${KEYWORDS[0]}"
	else
		NAME=$(IFS=_ ; echo "${KEYWORDS[*]}")
	fi
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

for kw in "${KEYWORDS[@]}"; do
	if [[ ! -d "$WAV_ROOT/$kw" ]]; then
		echo "Missing $WAV_ROOT/$kw/ — run sof_tflm_generate_keyword_dataset.sh" \
		     "--keyword <text> --label $kw first" >&2
		exit 1
	fi
done

# Step 1: silence + unknown (in system Python; no TF needed).
if [[ -z "${SKIP_PREP:-}" ]]; then
	echo ">>> [1/3] Preparing silence + unknown"
	"$SCRIPT_DIR/sof_tflm_prepare_silence_unknown.sh" "$WAV_ROOT"
else
	echo ">>> [1/3] Skipping silence/unknown prep (SKIP_PREP set)"
fi

# Step 2: feature extraction via testbench.
if [[ -z "${SKIP_FEATURES:-}" ]]; then
	echo ">>> [2/3] Emitting mel40 features via SOF testbench"
	"$SCRIPT_DIR/sof_mfcc_extract_features.sh" "$WAV_ROOT" "$FEAT_ROOT"
else
	echo ">>> [2/3] Skipping feature extraction (SKIP_FEATURES set)"
fi

# Step 3: train + quantize (needs an active TF venv).
if ! python3 -c 'import tensorflow' >/dev/null 2>&1; then
	cat >&2 <<'EOF'
tensorflow not importable — activate the tflm-train venv before running
this script, e.g.:

    python3 -m venv ~/venvs/tflm-train
    source ~/venvs/tflm-train/bin/activate
    pip install --upgrade pip
    pip install "tensorflow>=2.10" numpy

Then rerun this script from the same shell.
EOF
	exit 1
fi

echo ">>> [3/3] Training tiny_conv and int8-quantizing"
cd "$SCRIPT_DIR"
python3 sof_tflm_train.py \
	--feat-root "$FEAT_ROOT" \
	--labels silence unknown "${KEYWORDS[@]}" \
	--out-dir "$OUT_DIR" \
	--name "$NAME" \
	${EPOCHS:+--epochs "$EPOCHS"} \
	${BATCH_SIZE:+--batch-size "$BATCH_SIZE"} \
	${LR:+--lr "$LR"} \
	${WINDOW_HOP_STEP:+--window-hop-step "$WINDOW_HOP_STEP"} \
	${GAIN_AUG_DB_MIN:+--gain-aug-db-min "$GAIN_AUG_DB_MIN"} \
	${GAIN_AUG_DB_MAX:+--gain-aug-db-max "$GAIN_AUG_DB_MAX"}

LABEL_LIST=$(IFS=, ; echo "silence,unknown,${KEYWORDS[*]}")

cat <<EOF

>>> Model ready: $OUT_DIR

To wire it into SOF (speech.h #includes sof_tflm_labels.h so both the
model and its label set update automatically on rebuild — no source
edits are needed):
  cp $OUT_DIR/sof_tflm_quantized_model_data.{cc,h} \\
     $OUT_DIR/sof_tflm_labels.h \\
     $(cd "$SCRIPT_DIR"/.. && pwd)/

Archive artifacts (kept per-name for retraining history):
  $OUT_DIR/${NAME}_quantized_model.tflite
  $OUT_DIR/${NAME}_labels.txt

Label order (silence=0, unknown=1, keyword_i=i+2): ${LABEL_LIST}.
Every positive keyword lives at index >= 2, preserving the on-device
KPB max_idx >= 2 trigger rule.
EOF
