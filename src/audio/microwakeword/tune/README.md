# microWakeWord (MWW) Training & Tuning Toolchain

This directory contains offline scripts to train, quantize, verify, and export streaming keyword-spotting models for the SOF `microwakeword` (MWW) module using real SOF MFCC features extracted via `sof-testbench4`.

---

## Toolchain Overview

1. **Keyword Dataset Generation & Ingestion:**
   - [sof_mww_generate_keyword_dataset_from_dir.sh](sof_mww_generate_keyword_dataset_from_dir.sh): Ingests recorded real speech WAV samples from a directory, preserves clean reference clips, and complements them with Room Impulse Response (RIR) reverberation, additive background noise (at varying SNRs), tempo/pitch perturbations, and Gaussian gain jitter.
   - [sof_mww_generate_keyword_dataset_piper_tts.sh](sof_mww_generate_keyword_dataset_piper_tts.sh): Synthesizes positive keyword WAVs with single-speaker Piper TTS voices, pitch/tempo perturbation (via `sox`), Gaussian gain jitter, and Room Impulse Response (RIR) reverberation.
   - [sof_mww_generate_keyword_dataset.sh](sof_mww_generate_keyword_dataset.sh): Multi-speaker generation with `piper-sample-generator` (LibriTTS-R).
   - [sof_mww_prepare_silence_unknown.sh](sof_mww_prepare_silence_unknown.sh): Prepares negative dataset classes (`silence` and `unknown`) by slicing background noise and non-target words from Google Speech Commands v2.
   - [sof_mww_prepare_common_voice.py](sof_mww_prepare_common_voice.py): Selects a repeatable set of Mozilla Common Voice (CC0) sentence clips from extracted language releases into a `speech` class of continuous speech negatives, with a per-speaker limit and a transcript exclusion regex for the keyword. Common Voice is downloaded from Mozilla Data Collective.
   - [sof_mww_split_utterances.py](sof_mww_split_utterances.py): Splits recordings of a repeated keyword into one 48 kHz mono WAV per utterance. Speech is detected from the 100 - 7000 Hz band energy of 20 ms frames, and the room noise before and after each utterance is kept up to `--max-pad` and `--max-pad-after` seconds. Needs `scipy`.
   - [sof_mww_anonymize_clips.py](sof_mww_anonymize_clips.py): Copies keyword clips to `<label>_NNN.wav` names in a fixed-seed shuffled order, so the names do not reveal the speakers.
   - [sof_mww_asr_transcribe.py](sof_mww_asr_transcribe.py): Transcribes a directory of WAVs with an OpenVINO Whisper model (e.g. `whisper-medium-int4-ov`) into a `file,text` CSV, to find generated keyword clips that are not recognizable. Needs `openvino`, `transformers` and `soundfile`.
   - [sof_mww_asr_filter.py](sof_mww_asr_filter.py): Deletes the WAVs listed in a `sof_mww_asr_transcribe.py` CSV whose transcription does not match any `--pass` string, compared case-insensitively without punctuation. E.g. `python3 sof_mww_asr_filter.py --pass banana banana_asr.csv ~/train/mww_wov2/wavs/banana`. Use `--dry-run` to only report.

2. **DSP Feature Extraction:**
   - [sof_mfcc_extract_features.sh](sof_mfcc_extract_features.sh): Emits SOF MFCC hop records for a labelled WAV dataset by running the SOF host testbench with a 10 ms mel-40 topology: 184-byte hops ($24\text{-byte } \texttt{struct mfcc\_data\_header} + 40 \times \text{int32\_t}$ Q9.23 mel values) or with `--pcan` 64-byte hops ($24\text{-byte header} + 40 \times \text{int8\_t}$).

3. **Model Training & Export:**
   - [sof_mww_train_pcan.py](sof_mww_train_pcan.py): All-in-one PCAN (int8 feature) trainer working directly from WAVs. Features come from the SOF MFCC component run in `sof-testbench4` with `sof-hda-benchmark-mfccmel40_10ms_pcan32.tplg` (`--frontend sof`, default) or from TensorFlow `audio_microfrontend` (`--frontend tf`). Each clip is embedded in 2 s of continuous background from the ambient classes at 5..35 dB SNR with a random 0..29 ms phase, and the mixture is peak-normalized to a random level in `--level-min-dbfs`..`--level-max-dbfs` (default -45..-6 dBFS). Training windows are 160 hops so they cover the streaming model's 149-hop receptive field, and the evaluation streams each clip after background as on the device. The model input quantization is pinned to the firmware int8 PCAN scale (666/256, zero point -128). Digital silence (all-zero PCM, e.g. a muted or not yet started capture) is a hard negative (`--digital-silence N` streams, default 300): zeros only, background followed by zeros, and zeros followed by background. Windows from these streams are also trained from stream start, where the streaming model state is still zero, and the evaluation report includes a `digital_silence` class streamed from a state reset. The network is described in [model_architecture.md](model_architecture.md).
   - [sof_mww_int8_state.py](sof_mww_int8_state.py): Converts the float32 streaming state of a model (`.tflite` or `mww_model_data_<name>.h`) to int8 without changing any output, removing the per-inference DEQUANTIZE/QUANTIZE operations, and emits the header and the sof-ctl blob. `sof_mww_train_pcan.py` applies it to the models it exports; use it directly for older models. It also wraps a model in the old format without the `struct sof_mww_model_hdr` model header into the current format, and `--features`, `--threshold` and `--avg-len` change the header settings of a model.

4. **Automation:**
   - [sof_mww_pcan_train_fruits.sh](sof_mww_pcan_train_fruits.sh): Trains the strawberry, banana and orange PCAN models for the 3-keyword topology. Positives can optionally be ASR screened (`ASR_SCREEN=true`) on the clean TTS output before noise augmentation, and each fruit model gets the other fruits as `unknown` negatives since all detectors run at the same time. Real recordings of a keyword in `REAL_SAMPLES_ROOT/<label>/` are trimmed to the speech with 1 s of room noise before and 0.25 s after it, augmented with `sof_mww_generate_keyword_dataset_from_dir.sh` and added to the TTS positives. The trainer labels the keyword at the last detected speech of a clip, so a long pause after the word would teach the model to trigger late. `TRAIN_<FRUIT>=false` only generates the positives of a fruit for the cross-fruit negatives.

---

## Quickstart

### Environment Setup

#### Setup Piper Virtual Environment (for Dataset Generation)
```bash
python3 -m venv ~/venvs/piper
source ~/venvs/piper/bin/activate
pip install --upgrade pip
pip install piper-sample-generator
```

#### Setup `piper-sample-generator` and Model Checkpoint
```bash
git clone https://github.com/rhasspy/piper-sample-generator ~/git/piper-sample-generator
mkdir -p ~/git/piper-sample-generator/models
wget -O ~/git/piper-sample-generator/models/en_US-libritts_r-medium.pt \
      https://github.com/rhasspy/piper-sample-generator/releases/download/v2.0.0/en_US-libritts_r-medium.pt
```

#### Setup Model Training Virtual Environment (Python 3.10 via `uv`)
For systems with newer default Python versions (such as Ubuntu 24.04/26.04), install `uv` to manage a Python 3.10 environment required by TensorFlow 2.16:
```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
uv venv ~/venvs/mww-train --python 3.10
source ~/venvs/mww-train/bin/activate
uv pip install --upgrade pip
uv pip install "tensorflow==2.16.1" "tf-keras==2.16.0"
```

### 1. Generate Synthetic Keyword Data

#### Option A: Multi-speaker dataset with `piper-sample-generator` (PyTorch `.pt` model)
```bash
./sof_mww_generate_keyword_dataset.sh \
    --keyword "hey jarvis" \
    --label hey_jarvis \
    --venv ~/venvs/piper \
    ~/wov/wavs
```
By default, the script automatically uses:
- `PIPER_MODEL="$HOME/git/piper-sample-generator/models/en_US-libritts_r-medium.pt"` (auto-detected)
- `MAX_SAMPLES=1000` (clips synthesized per speaking-rate variation)
- `MAX_SPEAKERS=200` (cap on LibriTTS-R speaker indices for diverse, high-quality voices)
- `GAIN_AUG=1` (Gaussian level jitter enabled: peak target -10 dBFS, sigma 5 dB)

These can be overridden via environment variables if desired (e.g., `MAX_SAMPLES=1500 MAX_SPEAKERS=300 ./sof_mww_generate_keyword_dataset.sh ...`).

#### Option B: Single-speaker dataset with `piper-tts` (ONNX `.onnx` model)
```bash
./sof_mww_generate_keyword_dataset_piper_tts.sh \
    --keyword "hey jarvis" \
    --label hey_jarvis \
    --voice ~/.local/share/piper-voices/en_US-lessac-medium.onnx \
    ~/wov/wavs
```

#### Option C: Real speech dataset from a directory of recorded WAV samples
To use recorded human speech recordings instead of synthetic TTS:
```bash
./sof_mww_generate_keyword_dataset_from_dir.sh \
    --src-dir /path/to/recorded/audio \
    --label hi_intel \
    ~/wov/wavs
```
This converts the input audio to 16 kHz 16-bit mono, keeps the clean unperturbed samples, and automatically complements them with RIR convolution, background noise mixing (10–30 dB SNR), pitch/tempo shifts, and Gaussian level jitter to reach a full training set size (`MAX_SAMPLES=1000` by default).

---

### Training Multiple Keyword Variants into the Same Model

You can train a single wake-word model to trigger on multiple spoken variants (such as `"hey jarvis"` and `"hi jarvis"`) with **no increase in model size or DSP RAM**:

Generate datasets for both phrases, then pass both labels to `sof_mww_train_pcan.py` with repeated `--keyword` arguments (e.g. `--keyword hey_jarvis --keyword hi_jarvis --name hey_jarvis`). Both phrase directories are pooled as the positive class, and the exported model triggers when either phrase is spoken.

```bash
./sof_mww_generate_keyword_dataset.sh \
    --keyword "hey jarvis" --label hey_jarvis \
    --keyword "hi jarvis"  --label hi_jarvis \
    --venv ~/venvs/piper \
    ~/wov/wavs
```

---

### Training 8-bit PCAN Models with SOF Testbench Features

The firmware MWW models (`banana`, `hi_intel`, `orange`, `strawberry`) take int8 PCAN features from the `mel40_10ms_pcan_compress` MFCC profile. Train them with `sof_mww_train_pcan.py`, which computes the features with the SOF MFCC component in `sof-testbench4` so they are bit-exact with the firmware.

MFCC can alternatively produce int8 features with its soft AGC (AGC8, `mel40_10ms_agc8_compress` profile), a lighter frontend without PCAN. Train such models with `--features agc8`, which uses `sof-hda-benchmark-mfccmel40_10ms_agc832.tplg`, and build the WoV topology with `MWW_FEATURES=agc8`. A model works only with the feature type it was trained with.

#### 1. Build the Testbench and the PCAN Benchmark Topology
```bash
cd $SOF_WORKSPACE/sof
scripts/build-tools.sh
scripts/rebuild-testbench.sh
```
This provides:
- `tools/testbench/build_testbench/install/bin/sof-testbench4`
- `tools/build_tools/topology/topology2/development/sof-hda-benchmark-mfccmel40_10ms_pcan32.tplg`
- `tools/build_tools/topology/topology2/development/sof-hda-benchmark-mfccmel40_10ms_agc832.tplg`

#### 2. Prepare the Dataset
The trainer expects one WAV directory per class under the dataset root: the positive class named by `--keyword`, `unknown/` for non-target speech, and `ambient/` for silence, noise and babble. The ambient clips are also used as continuous background for every training clip.

```bash
cd src/audio/microwakeword/tune

# Positives from synthetic multi-speaker TTS (piper venv active) ...
MAX_SAMPLES=1000 ./sof_mww_generate_keyword_dataset.sh \
    --keyword banana --label banana ~/train/mww_banana/wavs

# ... or from recorded speech, one --src-dir per recording set
MAX_SAMPLES=3500 ./sof_mww_generate_keyword_dataset_from_dir.sh \
    --src-dir ~/rec/set1 --label hi_intel \
    --src-dir ~/rec/set2 --label hi_intel \
    ~/train/mww_hi_intel/wavs

# Negatives from Speech Commands v2, silence slices moved to ambient/
N_UNKNOWN=5000 N_SILENCE=1500 ./sof_mww_prepare_silence_unknown.sh ~/train/mww_banana/wavs
mkdir -p ~/train/mww_banana/wavs/ambient
mv ~/train/mww_banana/wavs/silence/*.wav ~/train/mww_banana/wavs/ambient/
rmdir ~/train/mww_banana/wavs/silence
```

Optional additions that improved the models:
- Multi-speaker cafeteria babble from the DEMAND `PCAFETER_16k` set (CC-BY-4.0) sliced into 5 s clips in `ambient/`. This reduced random false positives.
- For synthetic keywords, a few hundred clips per single-speaker Piper voice with `sof_mww_generate_keyword_dataset_piper_tts.sh --voice <model.onnx>`. Keep them around 20-30 % of the positives so the model does not learn the voices instead of the word.
- Competing wake phrases (`hey siri`, `ok google`, `alexa`, ...) synthesized with `sof_mww_generate_keyword_dataset_piper_tts.sh` into `unknown/` as hard negatives.
- Continuous sentence speech in `speech/`, e.g. 3000 Common Voice clips with `sof_mww_prepare_common_voice.py --out-dir ~/train/mww_hi_intel/wavs/speech ~/Music/CommonVoice/*/`. The `unknown/` clips are single words and only get windows near their start and end, while `speech/` clips are windowed over their whole length.

#### 3. Train
```bash
source ~/venvs/mww-train/bin/activate
python3 sof_mww_train_pcan.py \
    --wav-root ~/train/mww_banana/wavs \
    --keyword banana --name banana \
    --out-dir ~/train/mww_banana/model_pcan \
    --frontend sof \
    --level-min-dbfs -45 --level-max-dbfs -6 \
    --pos-level-copies 2 \
    --class-weight-neg 3.0 \
    --threshold 0.5 --avg-len 4 \
    --epochs 30
```
`--testbench` and `--tplg` default to the paths above under `$SOF_WORKSPACE`. `--threshold` and `--avg-len` are stored in the model blob header and used by the firmware trigger: it triggers when the mean of the last `--avg-len` probabilities reaches the threshold, and the streaming evaluation report uses the same rule.

#### 4. Deploy
```bash
cp ~/train/mww_banana/model_pcan/mww_model_data_banana.h $SOF_WORKSPACE/sof/src/audio/microwakeword/
cp ~/train/mww_banana/model_pcan/banana.txt $SOF_WORKSPACE/sof/tools/ctl/ipc4/mww/
```
The header is the built-in model selected by Kconfig. The `.txt` blob can be loaded at runtime with `sof-ctl`. Both contain the model with the `struct sof_mww_model_hdr` header, which carries the `--features`, `--threshold` and `--avg-len` settings of the training to the firmware trigger.

---

### Visualizing Streaming Diagnostics from mtrace

You can visualize real-time hop metrics, energy/noise floor, int8 feature quantization, and detection probability curves captured from device `mtrace` logs:

```bash
# Capture mtrace output from device:
sof-mtrace-reader > /tmp/mtrace.txt

# Plot streaming diagnostics:
./sof_mww_plot_mtrace.py /tmp/mtrace.txt -o /tmp/mww_diag.png
```
