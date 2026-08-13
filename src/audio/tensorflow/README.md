# TensorFlow Lite Micro (TFLM) & Wake-on-Voice (WoV) Architecture

This directory provides the TensorFlow Lite for Microcontrollers (TFLM) classification module (`TFLMCLY`) for Sound Open Firmware (SOF), including integration with MFCC feature extraction, `mtrace` stream shutdown summary logging, IPC host notifications, Key Phrase Buffer (KPB) Wake-on-Voice (WoV) trigger infrastructure, and abstracted audio input sources across **HDA**, **DMIC**, **SSP (I2S)**, and **SoundWire (ALH)**.

Two build modes are supported, selected automatically per target core:

- **LLEXT module** (HiFi4/HiFi5 targets: MTL, PTL, ARL, ...) — `CONFIG_COMP_TENSORFLOW=m`, built with Clang, linked against the HiFi4-optimized `nnlib-hifi4` kernels.
- **Statically linked into `zephyr.elf`** (HiFi3 targets with no LLEXT/module-manager support, e.g. TGL/cavs2.5) — `CONFIG_COMP_TENSORFLOW=y`, built with the Zephyr SDK GCC toolchain, `nnlib-hifi4` NOT built or linked (see [Build Instructions](#build-instructions) below).

---

## Overview

The TFLM module evaluates pre-trained micro speech neural network models inline within the SOF audio processing graph. It receives pre-processed audio feature tensors (mel-log spectrograms from the `mfcc` component), runs model inference in the **Data Processing (DP) domain**, logs keyword detections and stream shutdown event summaries to `mtrace`, issues IPC4 notifications to the host audio driver, and signals the KPB module to drain pre-roll audio history upon keyword detection.

---

## Architecture & Data Flow

### Dual-Path Wake-on-Voice (WoV) Architecture

To allow continuous keyword evaluation without streaming audio to the host until a keyword is detected, the pipeline separates real-time keyword detection from host PCM draining via KPB:

```mermaid
graph TD
    subgraph Audio_Inputs ["Abstracted Hardware DAI Input Sources"]
        HDA["HDA Analog Input (dai_type: HDA)"]
        DMIC["PCH DMIC Digital Mic (dai_type: DMIC)"]
        SSP["I2S / Bluetooth Codec (dai_type: SSP)"]
        SDW["SoundWire SmartMic (dai_type: ALH)"]
    end

    subgraph DAI_Abstraction ["Backend DAI Copier Widget"]
        DAI["dai-copier.1 (dai_type: $CAPTURE_DAI_TYPE)"]
    end

    subgraph KPB_Pipeline ["Capture & KPB Pipeline"]
        Gain["gain.2.1 (Volume Control)"]
        KPB["kpb.2.1 (Key Phrase Buffer)"]
    end

    subgraph Detect_Pipeline ["Real-Time Detection Path (KPB Pin 1) - DP Domain"]
        SRC["src.1.1 (Resampler: 48kHz -> 16kHz)"]
        MFCC["mfcc.1.1 (Mel-40 Feature Extractor)"]
        TFLM["tflmcly.1.1 (TFLM Keyword Classifier)"]
        DetHost["host-copier.1.capture (PCM device 1: arm/observe only, see Usage)"]
    end

    subgraph Host_Pipeline ["Host WoV Draining Path (KPB Pin 2)"]
        Host["host-copier.0.capture (PCM device 0: real drain target)"]
    end

    HDA --> DAI
    DMIC --> DAI
    SSP --> DAI
    SDW --> DAI

    DAI --> Gain
    Gain --> KPB
    KPB -- Pin 1: Live Audio --> SRC
    SRC --> MFCC
    MFCC -- Mel-40 Q9.23 Tensors --> TFLM
    TFLM --> DetHost

    TFLM -.->|1. KPB_EVENT_BEGIN_DRAINING Notifier| KPB
    TFLM -.->|2. IPC4 Notification (scaffolded, not wired up)| Host_Driver[Host Driver]
    TFLM -.->|3. Stream Shutdown Event Logging| MTrace[mtrace / Trace Log]

    KPB -- Pin 2: Pre-roll History Buffer --> Host
```

**Note on pipe 1's host-copier**: `host-copier.1.capture` (PCM device 1, "HDA Mic TFLM Detect") exists to *arm/instantiate* the detection pipeline and to give a host-visible device for debugging — it is **not** the WoV drain target. `arecord`-ing it will get an immediate `Input/output error` once its ring buffer underruns, because `tflmcly` (not the host) is the real consumer on this pipe; this is expected. The pipeline stays fully armed and running as long as the PCM is *opened*, whether or not a read ever succeeds — see [Usage](#usage) for how to hold it open for testing without triggering the read-error teardown. The actual pre-roll audio delivered to the host on keyword detection always arrives via `host-copier.0.capture` (PCM device 0).

---

## Domain Execution Model (LL vs. DP)

- **Low Latency (LL) Domain (Timer Task / 1ms tick loop)**:
  - Components: `dai-copier`, `eqiir`, `tdfb`, `drc`, `host-copier`
  - Purpose: Fixed 1 ms tick loop executed on Core 0 to meet hard real-time audio hardware deadlines.
  - Metrics: Reported via `ll_schedule.stats_report` (e.g. `ll core 0 timer avg 22,049 cycles (~56 µs)`).

- **Data Processing (DP) Domain (Asynchronous Task)**:
  - Components: `src`, `micsel`, `mfcc`, `tflmcly`
  - Purpose: Runs asynchronously in a background task whenever a new feature hop is produced by `mfcc` — measured empirically at roughly one inference every ~500ms (bring-up wall-clock gate, see [Known Limitations](#known-limitations--open-issues)).
  - **Separation**: Because TFLM runs in the DP domain, model inference cycles take place outside the 1 ms LL tick loop, guaranteeing zero impact on real-time LL audio latency or buffer overruns. On targets without a pre-existing DP scheduler user, `platform_init()` now calls `scheduler_dp_init()` explicitly so `mfcc`/`tflmcly` have somewhere to run (`src/platform/intel/cavs/platform.c`).

### MFCC frame format

Per hop, MFCC emits a 24-byte `struct mfcc_data_header` (magic/frame_number/reserved/energy/noise_energy/vad_flag) followed by `TFLM_FEATURE_SIZE` (40) `int32_t` Q9.23 mel-log values — `24 + 40*4 = 184` bytes (`MFCC_FRAME_BYTES` in the pipeline template). `tflm_process()` strips this header and requantizes each Q9.23 value into `int8_t` against the model's *real* input tensor `scale`/`zero_point` (read from the interpreter at prepare time, not assumed) before feeding the model's 49-hop sliding feature window.

---

## Known Limitations / Open Issues

- **Feature-representation mismatch (stock model only)**: the shipped 4-class `silence/unknown/yes/no` model was trained against TFLM's original `micro_speech` frontend, which applies a *nonlinear* PCAN auto-gain-control normalization before quantization. SOF's MFCC produces *linear* mel-log values. Empirically, real captured audio normalizes to `norm ≈ 0.03–1.5` (Q9.23 mel-log value / 2^23), while this model's actual `input_scale=0.101715`/`zero_point=-128` need `norm ≈ 0–26` to use its int8 dynamic range — so every real input saturates into the bottom ~8% of the range and the model outputs a flat, content-independent prediction. No linear rescale of `mfcc_mel_q23_to_int8()` fixes this; it needs either a model retrained directly on real SOF MFCC mel-log features (recommended — see [Training a Custom Keyword Model](#training-a-custom-keyword-model-with-piper-tts)), or a from-scratch PCAN-AGC-equivalent normalization stage ahead of quantization. **Training a new model on real SOF features, as described below, avoids this problem entirely** since train-time and inference-time feature extraction then match by construction.
- **500ms inference cadence is a bring-up shortcut**, not the model's trained stride (20ms/hop, 49-hop/~1s sliding window). Fine for initial bring-up; revisit before judging a new model's real-world accuracy, since a cadence mismatch vs. training assumptions can look like a model-quality problem.
- **IPC4 host notification is scaffolded but not wired up** (`tflm_ipc_notification_init()` never allocates/registers `cd->msg`, so `tflm_send_keyword_notification()` silently no-ops). KPB draining (a separate mechanism) still works. Finish this if the host driver needs an explicit "keyword X detected" event rather than just observing PCM start flowing on device 0.
- **Per-instance state is global**, not per-`comp_dev` (`g_tflm_cd`, `g_tflm_initialized`, per-category counters, etc.). Fine for a single detector instance; would need reworking for concurrent multi-`tflmcly` use.
- **Category count/labels are still hardcoded in a few places** beyond `TFLM_CATEGORY_DATA` (shutdown-summary format string, the KPB-trigger rule `max_idx >= 2`) — generalize before changing the category set.

---

## Abstracted Audio Input Sources in Topology v2

The TFLM Keyword Detection and KPB pre-roll pipeline is decoupled from the physical DAI input source using the generic `dai-copier` widget:

```conf
Object.Widget.dai-copier.1 {
    dai_type    $CAPTURE_DAI_TYPE     # "HDA", "DMIC", "SSP", or "ALH" (SoundWire)
    copier_type $CAPTURE_COPIER_TYPE  # "HDA", "DMIC", "SSP", or "ALH"
    stream_name $CAPTURE_DAI_NAME    # "Analog", "DMIC01", "SSP0", "SDW0-Capture"
    node_type   $CAPTURE_NODE_TYPE    # $HDA_LINK_INPUT_CLASS, $DMIC_LINK_INPUT_CLASS, etc.
}
```

Platform wrappers select the input source cleanly via configuration defines:
```conf
Define {
    CAPTURE_SOURCE "hda" # Options: "hda", "dmic", "ssp", "soundwire"
}

IncludeByKey.CAPTURE_SOURCE {
    "hda"       "platform/intel/capture-hda.conf"
    "dmic"      "platform/intel/capture-dmic.conf"
    "ssp"       "platform/intel/capture-ssp.conf"
    "soundwire" "platform/intel/capture-sdw.conf"
}
```

---

## Topology v2 Integration & Usage

### 1. Component Widget Definition (`include/components/tflm.conf`)

Defines `Class.Widget."tflmcly"`:
- **UUID**: `42:c6:1d:c5:e1:a2:df:48:a4:90:e2:74:8c:b6:36:3e` (`c51dc642-a2e1-48df-a490e2748cb6363e`)
- **Type**: `effect`

### 2. Detection Pipeline Template (`include/pipelines/cavs/host-gateway-micsel-mfcc-tflm-capture.conf`)

Instantiates the real-time detection graph — `mfcc.1` carries a real bytes-control config default (`HDA_MIC_MFCC_PARAMS`, defaulting to `include/components/mfcc/mel40_compress.conf`), and `tflmcly.1`'s output is routed to a real host-copier rather than a terminal virtual sink:

```conf
Object.Widget {
    host-copier."1" {
        type            "aif_out"
        node_type       $HDA_HOST_INPUT_CLASS
        stream_name     "HDA Mic TFLM Detect"
        pcm_id          $index
    }
    src."1"     { ... }
    mfcc."1"    {
        Object.Control.bytes."1" {
            name "HDA Mic MFCC bytes"
            IncludeByKey.HDA_MIC_MFCC_PARAMS {
                "default" "include/components/mfcc/mel40_compress.conf"
            }
        }
    }
    tflmcly."1" { scheduler_domain "DP" }
}

Object.Base.route [
    { source tflmcly.$index.1; sink "host-copier.$index.capture" }
]
```

### 3. Top-Level Topology Configuration (`sof-hda-generic.conf` + `hda-mic-tflm-kpb.conf` overlay)

Instantiates the complete HDA Mic WoV topology with dual-path KPB routing:
```conf
Object.Base.route [
    # DAI -> Gain -> KPB
    { source "dai-copier.HDA.Analog.capture"; sink "gain.2.1" }
    { source "gain.2.1"; sink "kpb.2.1" }

    # KPB Pin 1 -> Real-time Detection Path (DP Domain)
    { source "kpb.2.1"; sink "src.1.1" }

    # KPB Pin 2 -> Host WoV Draining Path
    { source "kpb.2.1"; sink "host-copier.0.capture" }
]
```

A "HDA Mic TFLM Detect" PCM entry (`$HDA_TFLM_DETECT_PIPELINE_ID`, mono S32_LE @16kHz) exposes pipe 1's host-copier as PCM device 1.

---

## Build Instructions

### HiFi4/HiFi5 targets (MTL, PTL, ARL, ...) — LLEXT module, Clang

```bash
source .venv/bin/activate
export LLVM_TOOLCHAIN_PATH=<path-to-llvm-install>
west build -b intel_adsp/<board> app -d build-<board>-tflm -- \
    -DCONFIG_COMP_TENSORFLOW=m
```

### HiFi3 targets with no LLEXT support (TGL/cavs2.5) — static link, GCC

Requires `CONFIG_COMP_TENSORFLOW=y` (not `m`) plus C++17 and enough stack/heap for the interpreter's arena, TFLM's own allocations, and `avcodec`-style blocking calls off the DP task — all already added to `app/boards/intel_adsp_cavs25.conf` on this branch:

```conf
CONFIG_SOF_STAGING=y
CONFIG_CPP=y
CONFIG_STD_CPP17=y
CONFIG_COMP_TENSORFLOW=y
CONFIG_STACK_SIZE_EDF=32768
CONFIG_HEAP_MEM_POOL_SIZE=32768
CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=32768
```

Build with the Zephyr SDK GCC toolchain (no Clang/LLVM involved):

```bash
source .venv/bin/activate
export ZEPHYR_SDK_INSTALL_DIR=/home/lrg/zephyr-sdk-1.0.1
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
unset LLVM_TOOLCHAIN_PATH
west build -b intel_adsp/cavs25 app -d build-tgl-tflm-gcc
```

`src/audio/tensorflow/CMakeLists.txt` auto-detects that this target has no `nnlib-hifi4` support (`CONFIG_XTENSA_HIFI4` unset, even though the compiler may still be Clang for other targets) and skips building/linking `nn_hifi_lib`, falling back to plain-C TFLM reference kernels. It also extracts just the `abs()`/libm archive members TFLM needs from the toolchain's `libc.a` into a small private `tflm_libc_shim`, since `CONFIG_MINIMAL_LIBC` lacks them and linking the whole `libc.a` collides with Zephyr's own `malloc`/`free`.

### Building topology targets

From `sof/tools/build_tools`:

```bash
# Build HDA TFLM KPB Topologies (MTL / PTL / TGL)
ninja topology2_prod_sof-mtl-hda-tflm-kpb
ninja topology2_prod_sof-ptl-hda-tflm-kpb
ninja topology2_prod_sof-tgl-hda-tflm-kpb

# Build SoundWire TFLM Topology (ARL-S)
ninja topology2_dev_sof-arl-cs42l43-l0-cs35l56-l23-mfcc-mel-normal
```

---

## Usage

### PCM device map (`sof-hda-tflm` topology)

| Device | Widget | Role |
|---|---|---|
| `hw:0,0` | `host-copier.0.capture` | Real WoV output: KPB pin 2's pre-roll drain target. Reading this exercises the full `dai-copier -> gain -> kpb -> host` chain and is the right place to capture the actual detected/drained audio. |
| `hw:0,1` | `host-copier.1.capture` ("HDA Mic TFLM Detect") | Arms/observes the detection pipeline only — see the [pipeline diagram note](#dual-path-wake-on-voice-wov-architecture). Not a continuous PCM stream; see below. |

### Sanity-checking the WoV drain path end-to-end

```bash
ssh root@<dut> 'arecord -D hw:0,0 -f S32_LE -r 48000 -c 2 -d 4 /tmp/sanity.wav'
```

A clean, error-free capture here confirms the physical DAI, gain, and KPB chain are all healthy independent of TFLM/MFCC.

### Arming the detection pipeline and watching inferences

`hw:0,1` is not meant to be read continuously: `tflmcly` (not the host) is the real consumer on this pipe, so `arecord`'s first failed `read()` tears the pipeline straight back down. To arm it and hold it running for observation, open the PCM directly via `libasound` without ever reading from it — e.g. a small ctypes/C snippet calling `snd_pcm_open()` + `snd_pcm_set_params()` + `snd_pcm_start()` on `hw:0,1` and then just sleeping. In parallel, tail `mtrace` on the DUT:

```bash
ssh root@<dut> '/usr/local/bin/mtrace-reader.py' > /tmp/mtrace.log &
# ... arm hw:0,1 and play/speak keywords into the mic ...
# look for periodic "[TFLM PREPARE]", "[DBG hop]", "[DBG raw_output]" lines
```

On a high-confidence detection, `KPB_EVENT_BEGIN_DRAINING` fires and pre-roll history starts flowing via `host-copier.0.capture` — i.e. it shows up on `hw:0,0`, not `hw:0,1`.

### Stream shutdown summary

Upon stream reset or module destruction (`tflm_reset()` / `tflm_free()`), TFLM emits a summary to `printk`/`mtrace`:

```text
[TFLM STREAM SHUTDOWN SUMMARY] Total Inferences=142 | Keyword Events: Silence=120, Unknown=18, Yes=3, No=1 | Total KPB Triggers=4
```

- `Total Inferences`: cumulative classification inferences completed during the stream session.
- `Keyword Events`: per-category classification breakdown.
- `Total KPB Triggers`: high-confidence detections that triggered `KPB_EVENT_BEGIN_DRAINING`.

---

## Training a Custom Keyword Model with Piper-TTS

The stock model only recognizes `yes`/`no` (plus `silence`/`unknown`). The
shipped `sof_tflm_quantized_model_data.{cc,h}` was retrained end-to-end
against real SOF mel40 features (currently against the `hey_linux`
keyword) using the scripts under [./tune/](./tune/). This section
documents that exact recipe so the model can be reproduced, a different
keyword substituted, or several keywords combined into one model.

### Pipeline overview

The training flow is orchestrated by
[sof_tflm_train_pipeline.sh](./tune/sof_tflm_train_pipeline.sh), which
chains four steps into one command:

| Step | Script | What it does |
|------|--------|-------------|
| 0a | [sof_tflm_generate_keyword_dataset.sh](./tune/sof_tflm_generate_keyword_dataset.sh) | Synthesize `<label>/*.wav` for one English keyword with `piper-sample-generator` (multi-speaker LibriTTS-R) + augmentation |
| 0b | [sof_tflm_generate_keyword_dataset_piper_tts.sh](./tune/sof_tflm_generate_keyword_dataset_piper_tts.sh) | Same output layout, but for any single-speaker Piper voice (Finnish, Swedish, Hungarian, German, ...). Recovers speaker diversity via per-utterance prosody randomization + sox pitch/tempo perturbation |
| 1 | [sof_tflm_prepare_silence_unknown.sh](./tune/sof_tflm_prepare_silence_unknown.sh) | Slice `silence/` + sample `unknown/` from Speech Commands v2 (English is fine as a negative-class source even for non-English keywords) |
| 2 | [sof_mfcc_extract_features.sh](./tune/sof_mfcc_extract_features.sh) | Emit SOF mel40 features via `sof-testbench4` |
| 3 | [sof_tflm_train.py](./tune/sof_tflm_train.py) | Train `tiny_conv`, int8-quantize, write `.tflite` + C array |

Off-device verification uses
[sof_tflm_verify.py](./tune/sof_tflm_verify.py), which runs the quantized
`.tflite` on the held-out validation split and prints a confusion
matrix — catches models with good overall accuracy but poor keyword
recall before flashing.

The labels are locked at `silence(0)`, `unknown(1)`, `<keyword_1>(2)`,
`<keyword_2>(3)`, ... so the on-device tflmcly KPB trigger rule
(`max_idx >= 2`) fires for any positive keyword class.

### 0. Prerequisites

Two isolated Python venvs (Piper's `torch`/`librosa` stack and
TensorFlow do not coexist cleanly):

```bash
# Piper venv (dataset generation)
python3 -m venv ~/venvs/piper
source ~/venvs/piper/bin/activate
pip install --upgrade pip
pip install piper-sample-generator

# TFLM training venv
python3 -m venv ~/venvs/tflm-train
source ~/venvs/tflm-train/bin/activate
pip install --upgrade pip
pip install "tensorflow>=2.10" numpy
```

Fetch the Piper LibriTTS-R generator checkpoint once (one file supplies
up to 904 speaker embeddings — more voice diversity than a bag of
individual `.onnx` voices):

```bash
git clone https://github.com/rhasspy/piper-sample-generator ~/git/piper-sample-generator
mkdir -p ~/git/piper-sample-generator/models
wget -O ~/git/piper-sample-generator/models/en_US-libritts_r-medium.pt \
  https://github.com/rhasspy/piper-sample-generator/releases/download/v2.0.0/en_US-libritts_r-medium.pt
```

The training venv also needs the SOF host tools:

- `sof-testbench4` built at
  `tools/testbench/build_testbench/install/bin/sof-testbench4`
  (`scripts/rebuild-testbench.sh`).
- The mel40 development topology
  `sof-hda-benchmark-mfccmel4032.tplg` built via
  `scripts/build-tools.sh -t`.
- `sox` and `xxd` on `PATH`.

Export `SOF_WORKSPACE` to point at the parent of the `sof/` tree —
[sof_mfcc_extract_features.sh](./tune/sof_mfcc_extract_features.sh) uses
it to locate the testbench and topology binaries.

### 1. Generate the keyword dataset

[sof_tflm_generate_keyword_dataset.sh](./tune/sof_tflm_generate_keyword_dataset.sh)
synthesizes one keyword phrase across 3 speaking rates × up to 700
speaker embeddings, then augments the clips with room impulse
responses, volume jitter, and 16 kHz resampling. Rerun once per
keyword you want in the model. `MAX_SAMPLES=1000` (~3000 raw clips,
~3000 augmented after the pass) is the recommended setting — large
enough to train a robust model without the multi-hour training time
that `MAX_SAMPLES=2000` incurs. The built-in default of 400 is only
enough to smoke-test the pipeline:

```bash
source ~/venvs/piper/bin/activate
MAX_SAMPLES=1000 \
PIPER_VENV=~/venvs/piper \
PIPER_MODEL=~/git/piper-sample-generator/models/en_US-libritts_r-medium.pt \
PIPER_REPO=~/git/piper-sample-generator \
    ./sof_tflm_generate_keyword_dataset.sh --keyword "hey linux" ~/wov/wavs
deactivate
```

Output lands at `~/wov/wavs/hey_linux/*.wav`. The subdir name is
derived from `--keyword` by lower-casing and replacing spaces with
underscores; override with `--label mykw` if desired. For a
multi-keyword model, pass `--keyword` multiple times in a single call
(and optionally one `--label` per keyword, paired positionally):

```bash
MAX_SAMPLES=1000 PIPER_VENV=~/venvs/piper \
PIPER_MODEL=~/git/piper-sample-generator/models/en_US-libritts_r-medium.pt \
PIPER_REPO=~/git/piper-sample-generator \
    ./sof_tflm_generate_keyword_dataset.sh \
        --keyword banana --keyword mango --keyword orange \
        ~/wov/wavs
```

`PIPER_REPO` is required because upstream ships the `piper_train`
package only in the git tree, not in the PyPI wheel.

### 1b. Non-English keywords: single-speaker Piper voices

`piper-sample-generator` only ships the English `en_US-libritts_r-medium`
multi-speaker checkpoint. For a keyword in another language use
[sof_tflm_generate_keyword_dataset_piper_tts.sh](./tune/sof_tflm_generate_keyword_dataset_piper_tts.sh),
which drives the regular `piper-tts` package against any voice from the
[rhasspy/piper-voices](https://huggingface.co/rhasspy/piper-voices) set
(Finnish `fi_FI-harri`, Swedish `sv_SE-nst`, Hungarian `hu_HU-anna`,
German `de_DE-thorsten`, etc.). Because those voices are single-speaker,
this variant recovers diversity by (a) randomizing Piper's
`--noise-scale` / `--noise-w` per utterance while cycling several
`--length-scale` values, and (b) fanning each synthesized clip out into
`PERTURB_PER_UTT` sox copies with a random pitch shift in cents and a
pitch-preserving `tempo -s` factor.
Set up the venv once:

```bash
python3 -m venv ~/venvs/piper-tts
source ~/venvs/piper-tts/bin/activate
pip install --upgrade pip
pip install piper-tts
```

Fetch one voice per language (example: Finnish, Swedish, Hungarian):

```bash
BASE=https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0
mkdir -p ~/git/piper-voices/fi_FI ~/git/piper-voices/sv_SE ~/git/piper-voices/hu_HU
cd ~/git/piper-voices/fi_FI
wget "$BASE/fi/fi_FI/harri/medium/fi_FI-harri-medium.onnx"
wget "$BASE/fi/fi_FI/harri/medium/fi_FI-harri-medium.onnx.json"
cd ~/git/piper-voices/sv_SE
wget "$BASE/sv/sv_SE/nst/medium/sv_SE-nst-medium.onnx"
wget "$BASE/sv/sv_SE/nst/medium/sv_SE-nst-medium.onnx.json"
cd ~/git/piper-voices/hu_HU
wget "$BASE/hu/hu_HU/anna/medium/hu_HU-anna-medium.onnx"
wget "$BASE/hu/hu_HU/anna/medium/hu_HU-anna-medium.onnx.json"
```

Generate the Finnish "piparkakku" dataset:

```bash
source ~/venvs/piper-tts/bin/activate
MAX_SAMPLES=1000 \
PIPER_VOICE=~/git/piper-voices/fi_FI/fi_FI-harri-medium.onnx \
PIPER_REPO=~/git/piper-sample-generator \
    ./sof_tflm_generate_keyword_dataset_piper_tts.sh \
        --keyword piparkakku ~/wov/wavs
deactivate
```

`PIPER_REPO` is optional here: if set, the same IR-convolution +
volume-jitter augment stage the English pipeline uses runs after
synthesis; if unset, the pipeline falls back to a plain 16 kHz resample
plus the built-in gain jitter. `PIPER_TTS_VENV` is an alternative to
sourcing `activate` yourself — the script only auto-activates when the
venv path actually contains `bin/activate`.

Mixed-language multi-keyword models work by pairing `--keyword` with
`--voice` positionally:

```bash
source ~/venvs/piper-tts/bin/activate
MAX_SAMPLES=1000 \
    ./sof_tflm_generate_keyword_dataset_piper_tts.sh \
        --keyword piparkakku  --voice ~/git/piper-voices/fi_FI/fi_FI-harri-medium.onnx \
        --keyword pepparkaka  --voice ~/git/piper-voices/sv_SE/sv_SE-nst-medium.onnx \
        --keyword mézeskalács --voice ~/git/piper-voices/hu_HU/hu_HU-anna-medium.onnx \
        ~/wov/wavs
deactivate
```

Everything downstream (silence/unknown from Speech Commands v2, feature
extraction, training) is identical to the English recipe — Speech
Commands v2 English clips remain a valid negative source.

### 2. End-to-end pipeline: silence/unknown → features → train → quantize

The remaining three steps run in one command. From
`src/audio/tensorflow/tune/`, mirroring the shipped-model recipe
(`N_SILENCE=1500`, `N_UNKNOWN=4000`, `EPOCHS=40` — larger than the
built-in defaults of 500 / 1500 / 25):

```bash
N_SILENCE=1500 N_UNKNOWN=4000 EPOCHS=40 \
TFLM_VENV=~/venvs/tflm-train \
    ./sof_tflm_train_pipeline.sh --keyword hey_linux \
        ~/wov/wavs ~/wov/feats ~/wov/model
```

This will:

1. Fetch Speech Commands v2 (2.4 GB, one-time, cached under
   `~/.cache/speech_commands_v2`) and populate `~/wov/wavs/silence/`
   (1500 × 1 s slices of `_background_noise_`) and
   `~/wov/wavs/unknown/` (4000 clips sampled from the non-target
   Speech Commands words).
2. Run `sof-testbench4` on every `<label>/*.wav` with the S32 mel40
   topology, writing `~/wov/feats/<label>/*.raw` (each hop = 24-byte
   `struct mfcc_data_header` + 40 × Q9.23 `int32_t`).
3. Load features via [sof_tflm_dataset.py](./tune/sof_tflm_dataset.py),
   train a `tiny_conv` (Reshape → DepthwiseConv2D → Flatten → Dense →
   Softmax; ~12 k params) for `EPOCHS` epochs, full-int8 quantize
   using the *real training-feature distribution* as the
   representative set (this is what the stock model got wrong — see
   the PCAN-AGC note above), and emit the fixed
   `~/wov/model/sof_tflm_quantized_model_data.{cc,h}` pair (with the
   C symbol `g_sof_tflm_quantized_model_data` that `speech.cc`
   #includes) plus archive artifacts
   `~/wov/model/<name>_quantized_model.tflite` (~16 KB) and
   `~/wov/model/<name>_labels.txt`.

Re-runs skip the expensive stages via env flags:

```bash
SKIP_PREP=1 SKIP_FEATURES=1 EPOCHS=40 \
TFLM_VENV=~/venvs/tflm-train \
    ./sof_tflm_train_pipeline.sh --keyword hey_linux \
        ~/wov/wavs ~/wov/feats ~/wov/model
```

Further training knobs: `BATCH_SIZE`, `LR`. To train a multi-keyword
model, pass `--keyword` for each positive class (label order defines
model output indices starting at 2, so all positive classes still
trigger the KPB `max_idx >= 2` rule):

```bash
./sof_tflm_train_pipeline.sh \
    --keyword banana --keyword mango --keyword orange \
    --name fruits \
    ~/wov/wavs ~/wov/feats ~/wov/model
```

`--name` sets the archive basename for the standalone `.tflite` and
`_labels.txt` artifacts (defaults to the single keyword when only one
is given, otherwise the keyword labels joined with `_`). The C symbol
and header names emitted for `speech.cc` are fixed at
`g_sof_tflm_quantized_model_data` /
`sof_tflm_quantized_model_data.{cc,h}` regardless of `--name`, so
retraining is a drop-in replacement.

### 3. Verify off-device before flashing

```bash
source ~/venvs/tflm-train/bin/activate
python3 sof_tflm_verify.py \
    --tflite ~/wov/model/<name>_quantized_model.tflite \
    --feat-root ~/wov/feats \
    --labels silence,unknown,<keyword>
```

Prints per-class precision/recall and the confusion matrix on the same
held-out validation split training used (deterministic split via
`--val-frac` / `--seed`). The `hey_linux` recall matters more than
overall accuracy — a model with 98 % accuracy but 60 % keyword recall
will feel broken on device.

### 4. Wire the model into SOF

```bash
cp ~/wov/model/sof_tflm_quantized_model_data.{cc,h} \
   ~/wov/model/sof_tflm_labels.h \
   src/audio/tensorflow/
```

[speech.cc](speech.cc) already `#include`s
`sof_tflm_quantized_model_data.h` and calls
`tflite::GetModel(g_sof_tflm_quantized_model_data)`.
[speech.h](speech.h) `#include`s `sof_tflm_labels.h`, which the training
script regenerates on every run with the correct `TFLM_CATEGORY_COUNT`
and `TFLM_CATEGORY_DATA`. No source changes are needed on retraining.

The archive `~/wov/model/<name>_labels.txt` is kept as a plain-text copy
of the label order (silence, unknown, keyword_1, …) for retraining
history.

The `.cc` file is already wired into [CMakeLists.txt](CMakeLists.txt)
and its LLEXT sibling under [llext/CMakeLists.txt](llext/CMakeLists.txt);
replacing the file in-place is enough.

### 5. Validate on hardware

Rebuild ([Build Instructions](#build-instructions)) and deploy the new
firmware plus the reference topology `sof-hda-generic-tflm-kpb.tplg`,
built from [sof-hda-generic.conf](../../../tools/topology/topology2/sof-hda-generic.conf)
with `HDA_MIC_TFLM_KPB_CAPTURE=true` layering in
[hda-mic-tflm-kpb.conf](../../../tools/topology/topology2/platform/intel/hda-mic-tflm-kpb.conf).
It exposes two extra capture PCMs on the HDA-generic tree, both
`S32_LE / 2ch / 16 kHz`:

| PCM | Stream name | Purpose |
|-----|-------------|---------|
| `hw:0,42` | "HDA Mic TFLM Detect" | Arms the KPB → SRC → MFCC → TFLM branch |
| `hw:0,41` | "HDA Mic WoV Capture" | Receives the 2 s KPB pre-roll drain on trigger |

Order matters: opening `hw:0,41` alone leaves KPB without a `sel_sink`
and prepare fails. Always start the Detect PCM first — that instantiates
`micsel` and lets KPB bind both sinks.

**Arm the detector** (Detect PCM run to `/dev/null` — its job is to
drive the TFLM inference, not to produce a file):

```bash
arecord -D hw:0,42 -f S32_LE -c 2 -r 16000 -d 30 /dev/null &
```

**Capture the wake-word drain** in a second shell. KPB drains
`TFLM_KPB_DRAIN_REQ_MS = 2000 ms` of pre-roll on trigger, then streams
real-time until the PCM stops, so oversize the ring buffer:

```bash
arecord -D hw:0,41 -f S32_LE -c 2 -r 16000 \
        -M -N --buffer-size=65536 \
        -d 20 wov.wav
```

Now speak the keyword (`"hey linux"`) once. In `mtrace`,
`tflmcly.tflm_notify_kpb` should log at the trigger instant and KPB
should log `kpb_init_draining: requested draining of 2000 [ms]` right
after. `wov.wav` grows past its 44-byte header — the first ~2 s is the
pre-roll history buffer, the remainder is real-time capture through the
end of the arecord duration. On the stream-shutdown summary,
`Total KPB Triggers` matches the number of accepted detections.

If `wov.wav` stays 44 bytes: check `mtrace` for the `tflm_notify_kpb`
line. If it's missing, TFLM never crossed the confidence threshold
(speak closer / retrain with more data). If it's there but no KPB
`received event` follows, `CONFIG_AMS` on the target does not match
what [tflm-classify.c](tflm-classify.c) was compiled against — rebuild
against the deployed `.config`.

For sanity checks and stress runs, also confirm the class picked in
`mtrace` prediction logs stays on `silence`/`unknown` for a control set
of unrelated speech and background noise.

---

## Source Files

- **[tflm-classify.c](tflm-classify.c)**: SOF module adapter implementation for TFLM (feature-window management, header stripping, requantization, KPB trigger, shutdown summary).
- **[speech.cc](speech.cc)** / **[speech.h](speech.h)**: TFLM C++ API bridge & micro speech tensor wrapper.
- **[CMakeLists.txt](CMakeLists.txt)**: LLEXT-vs-static build selection, HiFi4 nnlib gating, static-link libc shim.
- **[../mfcc/tune/setup_mfcc.m](../mfcc/tune/setup_mfcc.m)**: Octave script generating the MFCC config blobs (`mel40.conf`, `mel40_compress.conf`, etc.) referenced by the topology.
- **[tflm.conf](../../../tools/topology/topology2/include/components/tflm.conf)**: Topology v2 widget class definition.
- **[host-gateway-micsel-mfcc-tflm-capture.conf](../../../tools/topology/topology2/include/pipelines/cavs/host-gateway-micsel-mfcc-tflm-capture.conf)**: Detection pipeline template.
- **[hda-mic-tflm-kpb.conf](../../../tools/topology/topology2/platform/intel/hda-mic-tflm-kpb.conf)**: WoV overlay layered on `sof-hda-generic.conf` via `HDA_MIC_TFLM_KPB_CAPTURE=true`.
