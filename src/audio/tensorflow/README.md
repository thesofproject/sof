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

### 2. Detection Pipeline Template (`include/pipelines/cavs/host-gateway-src-mfcc-tflm-capture.conf`)

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

### 3. Top-Level Topology Configuration (`sof-hda-tflm.conf`)

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

The stock model only recognizes `yes`/`no` (plus `silence`/`unknown`). To recognize custom keywords, train a new model with the same `tiny_conv` DS-CNN shape `speech.cc`'s `MicroMutableOpResolver<4>` already supports (Reshape, FullyConnected, DepthwiseConv2D, Softmax) — **and train it directly against real SOF MFCC mel-log features**, not the stock TFLM frontend, to avoid the PCAN-AGC mismatch described above.

### 1. Generate a synthetic dataset with Piper-TTS

Getting hundreds of real speakers to say a made-up keyword isn't practical — generate it with TTS instead:

```bash
pip install piper-sample-generator
```

Two voice sources:
- Individual Piper voices (`.onnx` + `.onnx.json`, one voice = one speaker) — download 10-20 `en_US`/`en_GB` voices for variety.
- The LibriTTS-R "generator" checkpoint, which mixes speaker embeddings from up to 904 underlying speakers via `--max-speakers`/`--slerp-weights` — more voice diversity from a single model file; avoid the highest-numbered speaker indices (the tool's own docs warn these have few training samples and produce artifacts).

Generate per keyword, looping over voices and speaking rates so samples aren't all one speaker/prosody:

```bash
for voice in voices/*.onnx; do
  for scale in 0.9 1.0 1.1; do
    python3 -m piper_sample_generator "<keyword>" \
        --model "$voice" --max-samples 50 \
        --length-scales "$scale" \
        --output-dir "raw/<keyword>/"
  done
done
```

Augment with the tool's own augmentation pass — randomizes volume, convolves with room impulse responses, resamples to 16kHz:

```bash
python3 -m piper_sample_generator.augment --input-dir raw/<keyword> --output-dir data/<keyword>
```

Optionally mix in background noise (Speech Commands v2's `_background_noise_` clips work well) at a few SNR levels for extra robustness.

Target a low thousand positive clips per keyword spread across as many voices/speeds/rooms as practical — diversity matters more than raw count. Start with a few hundred to validate the pipeline end-to-end, check the confusion matrix, then scale up if accuracy/false-accept rate isn't good enough. Supplement with a small set of real human recordings if available, even just for held-out validation.

Reuse Speech Commands v2's `_background_noise_` clips for the `silence` class, and a sample of its other 30 words (or spare non-keyword Piper output) for `unknown`. All of it just needs to land as plain 16kHz mono 16-bit WAV under `data_dir/<label>/*.wav`, one subfolder per label.

### 2. Generate matching MFCC features for training

Since the target frontend is SOF's own `mel40`/`mel40_compress` MFCC profile (see [MFCC frame format](#mfcc-frame-format) above: 40 mel bins, 20ms hop, 30ms window, Q9.23 fixed point), run the same MFCC extraction used on-device (`src/audio/mfcc/tune/setup_mfcc.m`'s `mel40` profile, or an equivalent host-side mel-log extractor with matching bin count/hop/window) over the generated dataset instead of TFLM's stock frontend, so train-time and inference-time features actually match. Export as plain floating-point mel-log values (pre-quantization) — quantization to int8 happens once, at conversion time, using the real training-data value distribution (see step 4).

### 3. Train

Using the classic `tflite-micro` `train_micro_speech_model.ipynb` / `speech_commands/train.py` pipeline (`tiny_conv` architecture):

```bash
python3 train.py --data_dir=<custom_mel40_dir> --wanted_words=<kw1,kw2,...> \
    --silence_percentage=25 --unknown_percentage=25 \
    --preprocess=micro --window_stride=20 --model_architecture=tiny_conv \
    --quantize=1
```

Run ~15-20k steps; validate accuracy/confusion matrix on a held-out split before proceeding.

### 4. Freeze, quantize, convert, and integrate

Freeze the graph, full-int8 quantize using a representative sample of the *real training feature distribution* (not an assumed 0..1 range — this is exactly what caused the stock model's mismatch), convert to `.tflite`, then regenerate the C array the same way `micro_speech_quantized_model_data.cc` was produced (e.g. `xxd -i` or TFLM's own conversion script), giving e.g. `custom_kw_quantized_model_data.{cc,h}`.

Swap it in:
- Replace the `#include "micro_speech_quantized_model_data.h"` in `speech.cc` with the new header.
- Update `TFLM_CATEGORY_COUNT`/`TFLM_CATEGORY_DATA` in `speech.h` to the new label set (keep `silence`/`unknown` as indices 0/1 to preserve the existing KPB-trigger convention).
- Sanity-check off-device first (host x86 TFLM build, or a plain TFLite interpreter) against held-out real recordings, to isolate model-quality problems from firmware-integration problems before touching hardware.

### 5. Validate on hardware

Rebuild ([Build Instructions](#build-instructions)), deploy, and feed real audio containing the new keywords per [Usage](#usage) above — confirm `mtrace` prediction logs pick the right class with reasonable confidence on true positives, and stay on `silence`/`unknown` for a control set of unrelated speech/background noise.

---

## Source Files

- **[tflm-classify.c](tflm-classify.c)**: SOF module adapter implementation for TFLM (feature-window management, header stripping, requantization, KPB trigger, shutdown summary).
- **[speech.cc](speech.cc)** / **[speech.h](speech.h)**: TFLM C++ API bridge & micro speech tensor wrapper.
- **[CMakeLists.txt](CMakeLists.txt)**: LLEXT-vs-static build selection, HiFi4 nnlib gating, static-link libc shim.
- **[../mfcc/tune/setup_mfcc.m](../mfcc/tune/setup_mfcc.m)**: Octave script generating the MFCC config blobs (`mel40.conf`, `mel40_compress.conf`, etc.) referenced by the topology.
- **[tflm.conf](../../../tools/topology/topology2/include/components/tflm.conf)**: Topology v2 widget class definition.
- **[host-gateway-src-mfcc-tflm-capture.conf](../../../tools/topology/topology2/include/pipelines/cavs/host-gateway-src-mfcc-tflm-capture.conf)**: Detection pipeline template.
- **[sof-hda-tflm.conf](../../../tools/topology/topology2/sof-hda-tflm.conf)**: Top-level WoV topology configuration.
