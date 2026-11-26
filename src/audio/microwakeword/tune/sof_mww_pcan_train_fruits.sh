#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# Train the strawberry, banana and orange PCAN models for the 3-keyword
# topology: generate and ASR screen Piper positives, prepare negatives with
# the other fruits as unknown speech, train with SOF testbench features and
# deploy the model headers and sof-ctl blobs into the SOF tree.
#
# Set SOF_WORKSPACE to the parent of the sof tree. BUILD_TOOLS=false skips
# rebuilding the testbench and topologies.

set -euo pipefail

cd $SOF_WORKSPACE/sof
# Set BUILD_TOOLS=false where prebuilt testbench and topology are copied in
if ${BUILD_TOOLS:-true}; then
    rm -rf tools/build-tools
    scripts/build-tools.sh
    scripts/rebuild-testbench.sh
fi

SOF_TUNE_DIR="$SOF_WORKSPACE/sof/src/audio/microwakeword/tune"
WOV1="$HOME/train/mww_wov1"
WOV2="$HOME/train/mww_wov2"
WOV3="$HOME/train/mww_wov3"

# Workflow toggles
GENERATE_DATASET=true
PREPARE_NEGATIVES=true
TRAIN_PCAN=true
DEPLOY_MODEL=true

# int8 features from MFCC: "pcan" or "agc8" (soft AGC, set the same
# MWW_FEATURES in the WoV topology). They are computed by the SOF MFCC
# component in sof-testbench4 (bit-exact with firmware). Build both before
# training:
#   scripts/rebuild-testbench.sh
#   sof-hda-benchmark-mfccmel40_10ms_${FEATURES}32.tplg (topology2 development targets)
FEATURES=pcan
TESTBENCH="$SOF_WORKSPACE/sof/tools/testbench/build_testbench/install/bin/sof-testbench4"
TPLG="$SOF_WORKSPACE/sof/tools/build_tools/topology/topology2/development/sof-hda-benchmark-mfccmel40_10ms_${FEATURES}32.tplg"

# Random per-clip peak level range in dBFS, applied to all classes before
# feature extraction. The DUT detector input peaks around -25 dBFS for
# normal speech at a distance.
LEVEL_MIN_DBFS=-45
LEVEL_MAX_DBFS=-6
POS_LEVEL_COPIES=2

# Fruit selection gates (enable/disable individually)
STRAWBERRY=true
BANANA=true
ORANGE=true

# Train the model of an enabled fruit. With false, only the fruit's positives
# are generated, for the cross-fruit negatives of the trained fruits.
TRAIN_STRAWBERRY=true
TRAIN_BANANA=true
TRAIN_ORANGE=true

# Dataset preparation and training settings (aligned with hi_intel PCAN training)
ADD_BABBLE=true
BABBLE_SAMPLES=400
N_SILENCE=1500
N_UNKNOWN=5000
MAX_SAMPLES=1000
# Single-speaker Piper voices added to the multi-speaker positives
# (3 x MAX_SAMPLES). Keep the total around 20-30 % so the model does not
# learn these few voices instead of the word.
ADD_SINGLE_SPEAKER=true
SINGLE_SPEAKER_SAMPLES=300
SINGLE_SPEAKER_VOICES=(
    "$HOME/git/piper-sample-generator/models/en_US-amy-medium.onnx"
    "$HOME/git/piper-sample-generator/models/en_US-lessac-medium.onnx"
    "$HOME/git/piper-sample-generator/models/en_US-ryan-medium.onnx"
)
EPOCHS=30
CLASS_WEIGHT_NEG=3.0
# Firmware trigger: mean of last DETECT_AVG_LEN probabilities >= DETECT_THRESHOLD
DETECT_THRESHOLD=0.5
DETECT_AVG_LEN=4

# Delete generated positives whose Whisper transcription is not the keyword
# (e.g. "banana" synthesized as "on Anna" or "oh no no"). Slow on CPU.
ASR_SCREEN=false
ASR_VENV=${ASR_VENV:-"$HOME/venvs/openvino_env"}
ASR_MODEL=${ASR_MODEL:-"$HOME/whisper/whisper-medium-int4-ov"}

# Real recordings of the keyword in $REAL_SAMPLES_ROOT/<label>/*.wav, if the
# directory exists. Each recording is expanded with pitch/tempo, reverb,
# noise and gain variation to REAL_SAMPLES clips, added to the TTS positives.
ADD_REAL_SAMPLES=true
REAL_SAMPLES_ROOT="$HOME/Music/local_mww_train_dataset"
REAL_SAMPLES=600
# The recordings are first trimmed around the detected speech. The trainer
# labels the keyword at the last detected speech, so keep the part after the
# word short. The splitter needs scipy, which the Piper venv may not have.
REAL_PAD_BEFORE=1.0
REAL_PAD_AFTER=0.25
REAL_TRIM_THRESHOLD_DB=12
SPLIT_PYTHON=${SPLIT_PYTHON:-/usr/bin/python3}

# The 3-keyword topology runs all fruit detectors at the same time, so add
# random positives of the other fruits to each fruit's unknown class.
CROSS_FRUIT_NEGATIVES=true
CROSS_FRUIT_SAMPLES=600

# Mozilla Common Voice sentences (CC0) as continuous speech negatives in
# wavs/speech/, one extracted language release per directory. Clips whose
# transcript contains the keyword are skipped.
ADD_COMMON_VOICE=true
COMMON_VOICE_SAMPLES=6000
COMMON_VOICE_MAX_PER_SPEAKER=5
shopt -s nullglob
COMMON_VOICE_DIRS=("$HOME"/Music/CommonVoice/*/)
shopt -u nullglob

# Optional Piper hard negatives; disabled by default (same as hi_intel script)
ADD_HARD_NEGATIVES=false
HARD_NEGATIVE_SAMPLES=200
HARD_NEGATIVE_PHRASES=(
    "hey siri"
    "hey google"
    "ok google"
    "alexa"
    "hey copilot"
    "hey jarvis"
)
PIPER_TTS_VENV=${PIPER_TTS_VENV:-"$HOME/venvs/piper"}
PIPER_VOICE=${PIPER_VOICE:-"$HOME/git/piper-sample-generator/models/en_US-lessac-medium.onnx"}
export PIPER_TTS_VENV

cd "$SOF_TUNE_DIR"

prepare_babble_cache() {
    BABBLE_CACHE="$HOME/.cache/demand_cafeteria"
    BABBLE_ZIP="$BABBLE_CACHE/PCAFETER_16k.zip"
    BABBLE_URL="https://zenodo.org/api/records/1227121/files/PCAFETER_16k.zip/content"

    mkdir -p "$BABBLE_CACHE"
    if [[ ! -f "$BABBLE_ZIP" ]]; then
        echo ">>> Downloading DEMAND cafeteria babble dataset (~100 MB, CC-BY-4.0)..."
        curl -L -f -o "$BABBLE_ZIP" "$BABBLE_URL" || wget -O "$BABBLE_ZIP" "$BABBLE_URL"
    fi
    if [[ ! -d "$BABBLE_CACHE/PCAFETER" ]]; then
        echo ">>> Unzipping DEMAND cafeteria noise into $BABBLE_CACHE/..."
        unzip -q -n "$BABBLE_ZIP" -d "$BABBLE_CACHE/"
    fi

    # Make cafeteria noise available to keyword augmentation when Speech
    # Commands has already been cached.
    SC_NOISE="$HOME/.cache/speech_commands_v2/_background_noise_"
    if [[ -d "$SC_NOISE" && ! -e "$SC_NOISE/demand_cafeteria.wav" ]]; then
        echo ">>> Linking DEMAND cafeteria noise into $SC_NOISE"
        ln -s "$BABBLE_CACHE/PCAFETER/ch01.wav" "$SC_NOISE/demand_cafeteria.wav"
    fi
}

add_cafeteria_babble() {
    local wav_root="$1"
    local -a babble_wavs
    local total_written=0
    local channel_index=0
    local slices_per_channel max_possible remaining to_extract i out_file

    if ! command -v sox >/dev/null 2>&1 || ! command -v soxi >/dev/null 2>&1; then
        echo "SoX and soxi are required to split DEMAND cafeteria babble" >&2
        return 1
    fi

    babble_wavs=("$BABBLE_CACHE"/PCAFETER/ch*.wav)
    if [[ ! -e "${babble_wavs[0]}" ]]; then
        echo "Warning: No ch*.wav found in $BABBLE_CACHE/PCAFETER" >&2
        return 0
    fi

    mkdir -p "$wav_root/ambient"
    rm -f "$wav_root/ambient"/cafe_ch*.wav
    slices_per_channel=$(((BABBLE_SAMPLES + ${#babble_wavs[@]} - 1) / ${#babble_wavs[@]}))

    for wav in "${babble_wavs[@]}"; do
        (( total_written >= BABBLE_SAMPLES )) && break

        # Source channels are 16 kHz mono. Match the hi_intel slicer: complete,
        # non-overlapping five-second (80,000-frame) chunks.
        max_possible=$(($(soxi -s "$wav") / 80000))
        remaining=$((BABBLE_SAMPLES - total_written))
        to_extract=$slices_per_channel
        if (( to_extract > max_possible )); then to_extract=$max_possible; fi
        if (( to_extract > remaining )); then to_extract=$remaining; fi

        for ((i = 0; i < to_extract; i++)); do
            printf -v out_file '%s/ambient/cafe_ch%02d_%04d.wav' \
                "$wav_root" "$((channel_index + 1))" "$i"
            sox "$wav" -r 16000 -c 1 -b 16 -e signed-integer \
                "$out_file" trim "$((i * 5))" 5
            total_written=$((total_written + 1))
        done

        channel_index=$((channel_index + 1))
    done

    echo "    Added $total_written cafeteria babble WAV clips to $wav_root/ambient/"
}

prepare_fruit() {
    local keyword="$1"
    local label="$2"
    local wav_root="$3"
    local train="${4:-true}"

    if $GENERATE_DATASET; then
        echo "=== Generating $keyword positive samples ==="
        rm -rf "$wav_root"
        MAX_SAMPLES="$MAX_SAMPLES" ./sof_mww_generate_keyword_dataset.sh \
            --keyword "$keyword" \
            --label "$label" \
            "$wav_root"
    fi

    if $GENERATE_DATASET && $ADD_SINGLE_SPEAKER; then
        local voice vname tmp_root wav
        for voice in "${SINGLE_SPEAKER_VOICES[@]}"; do
            vname=$(basename "$voice" .onnx)
            echo "=== Adding $SINGLE_SPEAKER_SAMPLES $keyword samples from $vname ==="
            tmp_root=$(mktemp -d -t "piper_${label}_${vname}.XXXXXX")
            MAX_SAMPLES="$SINGLE_SPEAKER_SAMPLES" \
                ./sof_mww_generate_keyword_dataset_piper_tts.sh \
                --keyword "$keyword" --label "$label" --voice "$voice" \
                "$tmp_root"
            # Clean pitch/tempo perturbed clips, same names as the augmented ones
            for wav in "$tmp_root/_perturbed/$label"/*.wav; do
                [[ -f "$wav" ]] || continue
                cp "$wav" "$wav_root/_raw/$label/${vname}_$(basename "$wav")"
            done
            for wav in "$tmp_root/$label"/*.wav; do
                [[ -f "$wav" ]] || continue
                mv "$wav" "$wav_root/$label/${vname}_$(basename "$wav")"
            done
            rm -rf "$tmp_root"
        done
    fi

    if $GENERATE_DATASET && $ASR_SCREEN; then
        # Transcribe the clean TTS output before noise and RIR augmentation so
        # only bad TTS speakers are dropped, not low SNR clips.
        echo "=== ASR screening of clean $keyword positives ==="
        local asr_csv="$wav_root/${label}_asr.csv"
        "$ASR_VENV/bin/python3" ./sof_mww_asr_transcribe.py --model "$ASR_MODEL" \
            "$wav_root/_raw/$label" "$asr_csv"
        python3 ./sof_mww_asr_filter.py --pass "$keyword" \
            "$asr_csv" "$wav_root/$label"
    fi

    if $GENERATE_DATASET && $ADD_REAL_SAMPLES && [[ -d "$REAL_SAMPLES_ROOT/$label" ]]; then
        echo "=== Adding $REAL_SAMPLES augmented clips of real $keyword recordings ==="
        local tmp_real wav src n
        local -a parts
        tmp_real=$(mktemp -d -t "real_${label}.XXXXXX")
        mkdir -p "$tmp_real/trimmed"
        for src in "$REAL_SAMPLES_ROOT/$label"/*.wav; do
            "$SPLIT_PYTHON" ./sof_mww_split_utterances.py -o "$tmp_real/split" \
                --threshold-db "$REAL_TRIM_THRESHOLD_DB" \
                --max-pad "$REAL_PAD_BEFORE" --max-pad-after "$REAL_PAD_AFTER" \
                "$src" > /dev/null
            n=$(basename "$src" .wav)
            parts=("$tmp_real/split/$n"/*.wav)
            if [[ -f "${parts[0]}" ]]; then
                # The keyword is the longest segment, others are clicks or breaths
                mv "$(ls -S "${parts[@]}" | head -1)" "$tmp_real/trimmed/$n.wav"
            else
                echo "Warning: no speech found in $src, used untrimmed" >&2
                cp "$src" "$tmp_real/trimmed/$n.wav"
            fi
        done
        MAX_SAMPLES="$REAL_SAMPLES" ./sof_mww_generate_keyword_dataset_from_dir.sh \
            --src-dir "$tmp_real/trimmed" --label "$label" "$tmp_real/out"
        for wav in "$tmp_real/out/$label"/*.wav; do
            [[ -f "$wav" ]] || continue
            mv "$wav" "$wav_root/$label/real_$(basename "$wav")"
        done
        rm -rf "$tmp_real"
    fi

    if $PREPARE_NEGATIVES && $train; then
        echo "=== Preparing silence, unknown speech, and babble for $keyword ==="
        mkdir -p "$wav_root/ambient"
        N_UNKNOWN="$N_UNKNOWN" N_SILENCE="$N_SILENCE" \
            ./sof_mww_prepare_silence_unknown.sh "$wav_root"

        if [[ -d "$wav_root/silence" ]]; then
            mv "$wav_root/silence"/*.wav "$wav_root/ambient/"
            rmdir "$wav_root/silence"
        fi

        if $ADD_BABBLE; then
            add_cafeteria_babble "$wav_root"
        fi

        if $ADD_COMMON_VOICE; then
            if (( ${#COMMON_VOICE_DIRS[@]} == 0 )); then
                echo "Error: no Common Voice releases in ~/Music/CommonVoice, set ADD_COMMON_VOICE=false to skip" >&2
                return 1
            fi
            echo "=== Selecting $COMMON_VOICE_SAMPLES Common Voice clips without \"$keyword\" ==="
            python3 ./sof_mww_prepare_common_voice.py \
                --out-dir "$wav_root/speech" \
                --num "$COMMON_VOICE_SAMPLES" \
                --max-per-speaker "$COMMON_VOICE_MAX_PER_SPEAKER" \
                --exclude "(?i)$keyword" \
                "${COMMON_VOICE_DIRS[@]}"
        fi
    fi

    if $ADD_HARD_NEGATIVES && $train; then
        if [[ -z "$PIPER_VOICE" ]]; then
            echo "PIPER_VOICE must point to a Piper .onnx voice when ADD_HARD_NEGATIVES=true" >&2
            return 1
        fi
        export PIPER_VOICE
        echo "=== Adding Piper hard negatives for $keyword ==="
        local tmp_hard_neg label_phrase index wav
        tmp_hard_neg=$(mktemp -d -t "piper_${label}_negative.XXXXXX")
        for index in "${!HARD_NEGATIVE_PHRASES[@]}"; do
            label_phrase=$(echo "${HARD_NEGATIVE_PHRASES[$index]}" | tr ' A-Z' '_a-z')
            MAX_SAMPLES="$HARD_NEGATIVE_SAMPLES" \
                ./sof_mww_generate_keyword_dataset_piper_tts.sh \
                --keyword "${HARD_NEGATIVE_PHRASES[$index]}" "$tmp_hard_neg"
            mkdir -p "$wav_root/unknown"
            for wav in "$tmp_hard_neg/$label_phrase"/*.wav; do
                [[ -f "$wav" ]] || continue
                mv "$wav" "$wav_root/unknown/hard_negative_${index}_$(basename "$wav")"
            done
        done
        rm -rf "$tmp_hard_neg"
    fi
}

if $ADD_BABBLE && { $PREPARE_NEGATIVES || $GENERATE_DATASET; }; then
    prepare_babble_cache
fi

if $GENERATE_DATASET; then
    source "$PIPER_TTS_VENV/bin/activate"
fi

if $STRAWBERRY; then prepare_fruit strawberry strawberry "$WOV1/wavs" $TRAIN_STRAWBERRY; fi
if $BANANA; then prepare_fruit banana banana "$WOV2/wavs" $TRAIN_BANANA; fi
if $ORANGE; then prepare_fruit orange orange "$WOV3/wavs" $TRAIN_ORANGE; fi

if $GENERATE_DATASET; then
    deactivate
fi

add_cross_fruit_negatives() {
    local keyword="$1"
    local wav_root="$2"
    local other other_root n

    mkdir -p "$wav_root/unknown"
    rm -f "$wav_root/unknown"/xfruit_*.wav
    for other in strawberry:$WOV1 banana:$WOV2 orange:$WOV3; do
        other_root="${other#*:}/wavs/${other%%:*}"
        other="${other%%:*}"
        [[ "$other" == "$keyword" || ! -d "$other_root" ]] && continue
        n=0
        while IFS= read -r wav; do
            cp "$wav" "$wav_root/unknown/xfruit_${other}_$(basename "$wav")"
            n=$((n + 1))
        done < <(find "$other_root" -maxdepth 1 -name '*.wav' | shuf -n "$CROSS_FRUIT_SAMPLES")
        echo "    Added $n $other clips to $wav_root/unknown/"
    done
}

if $CROSS_FRUIT_NEGATIVES; then
    echo "=== Adding cross-fruit negatives ==="
    if $STRAWBERRY && $TRAIN_STRAWBERRY; then add_cross_fruit_negatives strawberry "$WOV1/wavs"; fi
    if $BANANA && $TRAIN_BANANA; then add_cross_fruit_negatives banana "$WOV2/wavs"; fi
    if $ORANGE && $TRAIN_ORANGE; then add_cross_fruit_negatives orange "$WOV3/wavs"; fi
fi

if $TRAIN_PCAN; then
    for f in "$TESTBENCH" "$TPLG"; do
        if [[ ! -f "$f" ]]; then
            echo "Error: missing $f (needed for SOF PCAN features)" >&2
            exit 1
        fi
    done

    source "$HOME/venvs/mww-train/bin/activate"

    train_fruit() {
        local keyword="$1"
        local wav_root="$2"
        local out_dir="$3"
        local positive_count negative_count dir

        positive_count=$(find "$wav_root/$keyword" -maxdepth 1 -type f -name '*.wav' 2>/dev/null | wc -l)
        negative_count=0
        for dir in ambient silence unknown speech babble noise; do
            if [[ -d "$wav_root/$dir" ]]; then
                negative_count=$((negative_count + $(find "$wav_root/$dir" -maxdepth 1 -type f -name '*.wav' | wc -l)))
            fi
        done
        if (( positive_count == 0 || negative_count == 0 )); then
            echo "Error: incomplete dataset at $wav_root (positive $keyword WAVs: $positive_count; negative WAVs: $negative_count)." >&2
            echo "Enable GENERATE_DATASET to create the fruit positives, and ensure PREPARE_NEGATIVES completed." >&2
            return 1
        fi

        mkdir -p "$out_dir"
        python3 sof_mww_train_pcan.py \
            --wav-root "$wav_root" \
            --keyword "$keyword" \
            --name "$keyword" \
            --out-dir "$out_dir" \
            --frontend sof \
            --features "$FEATURES" \
            --testbench "$TESTBENCH" \
            --tplg "$TPLG" \
            --level-min-dbfs "$LEVEL_MIN_DBFS" \
            --level-max-dbfs "$LEVEL_MAX_DBFS" \
            --pos-level-copies "$POS_LEVEL_COPIES" \
            --class-weight-neg "$CLASS_WEIGHT_NEG" \
            --threshold "$DETECT_THRESHOLD" \
            --avg-len "$DETECT_AVG_LEN" \
            --epochs "$EPOCHS"

        if $DEPLOY_MODEL; then
            cp "$out_dir/mww_model_data_${keyword}.h" \
                "$SOF_WORKSPACE/sof/src/audio/microwakeword/"
            cp "$out_dir/${keyword}.txt" "$SOF_WORKSPACE/sof/tools/ctl/ipc4/mww/"
        fi
    }

    if $STRAWBERRY && $TRAIN_STRAWBERRY; then train_fruit strawberry "$WOV1/wavs" "$WOV1/model_pcan"; fi
    if $BANANA && $TRAIN_BANANA; then train_fruit banana "$WOV2/wavs" "$WOV2/model_pcan"; fi
    if $ORANGE && $TRAIN_ORANGE; then train_fruit orange "$WOV3/wavs" "$WOV3/model_pcan"; fi

    deactivate
fi
