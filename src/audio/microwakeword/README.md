# microWakeWord (MWW) Architecture & Multi-Slot WoV Integration

This directory provides the Sound Open Firmware (SOF) component wrapping [OHF-Voice's
microWakeWord](https://github.com/OHF-Voice/micro-wake-word) streaming
keyword-spotting model, integrated with MFCC feature extraction and the
multi-slot Key Phrase Buffer (KPB) Wake-on-Voice (WoV) arbitration infrastructure.

Unlike standard single-keyword detectors or 4-way softmax classifiers, microWakeWord
executes streaming inference over a stateful TensorFlow Lite Micro (TFLM) graph,
outputting a single sigmoid wake-word probability per keyword model.

In this architecture, **one unified codebase** supports **three concurrent keyword spotter instances**:
- **Slot 0** (Pipeline 111, legacy 101): `"strawberry"`
- **Slot 1** (Pipeline 112, legacy 102): `"hi intel"`
- **Slot 2** (Pipeline 113, legacy 103): `"orange"`

---

## Architecture & Data Flow

The multi-slot MWW topology for DMIC is defined in `tools/topology/topology2/platform/intel/dmic-wov-multi.conf` and used by the SoundWire (PTL, WCL), ADL rt5682 and HDA generic `wov-multi` topologies:

```mermaid
graph TD
    subgraph P119["Pipeline 119 — 16 kHz DMIC Capture  (Core 0, LL 1ms)"]
        DAI["dai-copier.DMIC.dmic16k.capture\n2/4ch · 16 kHz · S32_LE"]
    end

    subgraph P120["Pipeline 120 — Speaker Echo Reference  (Core 0, LL 1ms)"]
        SIGGEN["siggen.120.1\n(48 kHz stereo speaker playback,\nsilence when speaker is idle)"]
        SRC120["src.120.1\n(48 kHz -> 16 kHz · 2ch · S32_LE)"]
        SIGGEN --> SRC120
    end

    subgraph P115["Pipeline 115 — ECNS DP Processing  (Core 0, DP 20ms)"]
        ECNS["ecns.115.1\n(ECNS DP Module, 20ms = 320 frames)\nPin 0 In: Mics | Pin 1 In: Echo Ref\nMono, 100 Hz HPF, optional AEC"]
    end

    subgraph P116["Pipeline 116 — KPB History Buffer  (Core 0, LL 1ms)"]
        KPB["kpb.116.1\n(2.0s mono history = 64 KB)\n16 kHz · 1ch · S16_LE\nPin 0: sel_sink | Pin 1: host_sink"]
    end

    subgraph P118["Pipeline 118 — Detector Fanout  (Core 0, LL 1ms)"]
        FAN["module-copier.118.1\n(3-way fanout)"]
    end

    subgraph P111["Pipeline 111 — Slot 0: 'strawberry'"]
        MO101["module-copier.111.1"]
        MFCC0["mfcc.111.1\n(Mel-40 10ms PCAN Compress)\nIBS: 320B | OBS: 64B"]
        MWW0["mww.111.1\n(microWakeWord)\nModel: 'strawberry'"]
        MO101 --> MFCC0 --> MWW0
    end

    subgraph P112["Pipeline 112 — Slot 1: 'hi intel'"]
        MO102["module-copier.112.1"]
        MFCC1["mfcc.112.1\n(Mel-40 10ms PCAN Compress)\nIBS: 320B | OBS: 64B"]
        MWW1["mww.112.1\n(microWakeWord)\nModel: 'hi intel'"]
        MO102 --> MFCC1 --> MWW1
    end

    subgraph P113["Pipeline 113 — Slot 2: 'orange'"]
        MO103["module-copier.113.1"]
        MFCC2["mfcc.113.1\n(Mel-40 10ms PCAN Compress)\nIBS: 320B | OBS: 64B"]
        MWW2["mww.113.1\n(microWakeWord)\nModel: 'orange'"]
        MO103 --> MFCC2 --> MWW2
    end

    subgraph P114["Pipeline 114 — WOV Host PCM Capture  (Core 0, LL 1ms)"]
        ARB["wov-arbiter.114.1\n(4 input pins, 1 output pin\nPin 0: Audio | Pins 1-3: Features\n1ch mono 16 kHz S16_LE)"]
        HC12["host-copier.12\n(hw:0,12 · PCM 12 'DMIC Multi-WOV')\n1ch · 16 kHz · S16_LE / S32_LE"]
        ARB --> HC12
    end

    subgraph P101["Pipelines 101, 100 — Regular DMIC0 48k Host PCM Capture  (Core 0, LL 1ms)"]
        DAI48["dai-copier.DMIC.dmic01.capture\n(Pipeline 101 · 2/4ch · 48 kHz)"]
        GAIN101["gain.101.1\n(Dmic0 Capture Volume / Switch)"]
        IIR101["eqiir.101.1\n(40 Hz high-pass, +20 dB)"]
        MC101["module-copier.101.2"]
        HC10["host-copier.10\n(hw:0,10 · PCM 10 'DMIC Raw')\n2/4ch · 48 kHz · S32_LE"]
        DAI48 --> GAIN101 --> IIR101 --> MC101 --> HC10
    end

    DAI -- "Pin 0 In" --> ECNS
    SRC120 -- "Pin 1 In" --> ECNS
    ECNS -- "Pin 0 Out (Mono 16k)" --> KPB
    KPB -- "Pin 0 (sel_sink)" --> FAN
    FAN --> MO101
    FAN --> MO102
    FAN --> MO103

    KPB -- "Pin 1 (host_sink / drain audio)" --> ARB
    MWW0 -->|"Pin 1 (Features)"| ARB
    MWW1 -->|"Pin 2 (Features)"| ARB
    MWW2 -->|"Pin 3 (Features)"| ARB

    MWW0 -. "Notifier WOV_DETECT (slot=0)" .-> ARB
    MWW1 -. "Notifier WOV_DETECT (slot=1)" .-> ARB
    MWW2 -. "Notifier WOV_DETECT (slot=2)" .-> ARB
    MWW0 -. "Notifier KPB_CLIENT_EVT (DRAIN 2s)" .-> KPB
    MWW1 -. "Notifier KPB_CLIENT_EVT (DRAIN 2s)" .-> KPB
    MWW2 -. "Notifier KPB_CLIENT_EVT (DRAIN 2s)" .-> KPB
    ARB -. "Notifier WOV_CTRL (PAUSE/RESUME)" .-> MWW0
    ARB -. "Notifier WOV_CTRL (PAUSE/RESUME)" .-> MWW1
    ARB -. "Notifier WOV_CTRL (PAUSE/RESUME)" .-> MWW2

    style ECNS fill:#1b4f72,stroke:#555,color:#fff
    style KPB  fill:#1c4966,stroke:#555,color:#fff
    style MFCC0 fill:#7d6608,stroke:#555,color:#fff
    style MFCC1 fill:#7d6608,stroke:#555,color:#fff
    style MFCC2 fill:#7d6608,stroke:#555,color:#fff
    style MWW0 fill:#922b21,stroke:#555,color:#fff
    style MWW1 fill:#b7950b,stroke:#555,color:#fff
    style MWW2 fill:#d35400,stroke:#555,color:#fff
    style ARB  fill:#4a235a,stroke:#555,color:#fff
    style HC12 fill:#2d5a27,stroke:#555,color:#fff
    style HC10 fill:#1e8449,stroke:#555,color:#fff
```

The 48 kHz DMIC capture does not pass through ECNS. It is the same regular
DMIC0 capture of `dmic-generic.conf` as in the non-WoV topologies, without
TDFB/DRC (`DMIC0_ENHANCED_CAPTURE` false). The IDs above are for SoundWire;
HDA generic uses pipelines 12/11 and PCM 6, ADL rt5682 uses pipelines 14/13
and PCM 99. The speaker
playback is routed into `siggen.120.1` by the platform topology
(`SDW_SPK_ECHO_REF_ECNS`, `SPEAKER_ECHO_REF_ECNS` or `HDA_SPEAKER_ECHO_REF_ECNS`).

The SoundWire headset WoV (`sdw-jack-wov-multi.conf`, `INCLUDE_SDW_JACK_WOV=true`)
has no ECNS: `module-copier.11.0` (Jack In, 48 kHz 2ch) -> `micsel.140.1`
(`(L+R)/2` mono) -> `src.140.1` (48 -> 16 kHz) -> `module-copier.140.1`
(to 16-bit) -> `kpb.146.1` -> detector slots 141-143 -> `wov-arbiter.144.1` ->
PCM 14 `Headset Multi-WOV`.

---

## Cross-Platform Manifest Targets

The standalone DMIC-only 4-channel manifests (`dmic-wov-multi-ptl-4ch-manifest.conf`,
`dmic-wov-multi-4ch-manifest.conf`) and their `sof-*-dmic-wov-multi-4ch.tplg` targets
have been removed, because ECNS requires a speaker echo reference input that a
DMIC-only topology cannot provide. See
[`tools/topology/topology2/README.md`](../../../tools/topology/topology2/README.md)
for the remaining multi-slot WoV topology targets.

---

## Multi-Instance Design & Memory Management

### 1. Single Codebase, Three Isolated Instances
Previous implementations relied on global static variables, limiting execution to one model. The updated `mww_model.cc` introduces explicit instance tracking:

```cpp
constexpr int kMaxSlots = 3;

struct mww_instance {
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;
    TfLiteTensor* input = nullptr;
    TfLiteTensor* output = nullptr;
    float input_scale = 0.0f;
    int32_t input_zero_point = 0;
    bool initialized = false;
};

static struct mww_instance g_instances[kMaxSlots];
```

### 2. Static Arena Pre-Allocation (Zero Runtime Heap Fragmentation)
Each slot is allocated a dedicated 96 KB tensor arena in the DSP BSS section:

```cpp
alignas(16) static uint8_t g_arenas[kMaxSlots][98304];
```
This guarantees deterministic execution, avoids heap fragmentation, and ensures the DSP never encounters runtime out-of-memory errors during multiple wake-word operations.

### 3. Keyword Model Dispatch
Default built-in models are bound automatically by slot/pipeline index when using `CONFIG_COMP_MWW_MODEL_PIPELINE_ID` (default):
- **Slot 0** (`pipeline_id == 111` or `101`): `mww_model_data_strawberry`
- **Slot 1** (`pipeline_id == 112` or `102`): `mww_model_data_hi_intel`
- **Slot 2** (`pipeline_id == 113` or `103`): `mww_model_data_orange`

Alternatively, a single built-in model can be forced via Kconfig (`CONFIG_COMP_MWW_MODEL_HI_INTEL`, `CONFIG_COMP_MWW_MODEL_HEY_JARVIS`, `CONFIG_COMP_MWW_MODEL_STRAWBERRY`, `CONFIG_COMP_MWW_MODEL_ORANGE`, or `CONFIG_COMP_MWW_MODEL_BANANA`), omitting all other models from the build to reduce firmware image size.

Each instance can also be dynamically customized via SOF IPC4 `LARGE_CONFIG_SET` / bytes control or LLEXT model loading.

### 4. Model Blob Format
The built-in models and the bytes control use the same model blob: `struct sof_mww_model_hdr` from [user/mww.h](../../include/user/mww.h) followed by the TFLite flatbuffer. In the bytes control the blob follows the standard `struct sof_abi_hdr`. The 64-byte header carries:
- magic `MWWM`, version and header size, so that fields can be added later,
- the model size and its CRC32, checked before the model is used, and the needed tensor arena size (0 if unknown),
- the keyword name, for information,
- the feature type (PCAN or AGC8 int8 Mel), Mel band count, hop length and hops per inference; the firmware rejects models whose feature geometry it does not support,
- the trigger threshold (Q0.8) and the number of averaged probabilities, so each model carries its own trigger tuning,
- reserved words.

A bare TFLite flatbuffer without the header is still accepted and uses the default trigger settings `MWW_DETECT_THRESHOLD_Q8` and `MWW_DETECT_AVG_LEN`.

---

## Notifier Inter-Module Signaling Contract

The MWW detector participates in SOF's synchronous intra-DSP publish/subscribe notifier bus:

1. **`NOTIFIER_ID_WOV_DETECT`**: Fired by `mww` to `wov_arbiter` on keyword detection (average probability of the last `avg_len` inferences `>= threshold_q8`, from the model header). Passes payload `{ .slot_id = cd->wov_slot_id }`.
2. **`NOTIFIER_ID_KPB_CLIENT_EVT`**: Fired by `mww` to `kpb` to request immediate draining of the 2.0 s pre-roll history buffer.
3. **`NOTIFIER_ID_WOV_CTRL`**: Received by `mww` from `wov_arbiter`.
   - `WOV_ARB_CMD_PAUSE`: Pauses inference while another slot's keyword audio is streaming to the host.
   - `WOV_ARB_CMD_RESUME`: Re-arms the detector when the host closes or resets the capture stream.

---

## ALSA Kcontrols & Userspace Verification

### 1. Active Slot Status (`DMIC Phrase Detect ID` / `Jack In Phrase Detect ID`)
The arbiter exposes a volatile read-only ALSA enum kcontrol:
```bash
# For DMIC WoV:
amixer -c 0 cget name='DMIC Phrase Detect ID'
# For SDW Headset WoV:
amixer -c 0 cget name='Jack In Phrase Detect ID'
# Values: 0 = Listening, 1 = Slot 1 ("strawberry"), 2 = Slot 2 ("hi intel"), 3 = Slot 3 ("orange")
```

### 2. Audio Capture Endpoints
- **DMIC 48 kHz Stream** (PCM 10 `DMIC Raw` on SoundWire, PCM 6 on HDA generic, PCM 99 on ADL rt5682; 40 Hz high-pass, no ECNS): `arecord -D hw:0,10 -f S32_LE -r 48000 -c 4 -d 5 /tmp/dmic48k.wav` (use `-c 2` with `NUM_DMICS=2`).
- **Mono WOV Capture Stream** (PCM 12): `arecord -D hw:0,12 -f S16_LE -r 16000 -c 1 -d 5 /tmp/wov.wav` (supports both `S16_LE` and `S32_LE`).
- **Headset Mono WOV Capture Stream** (PCM 14): `arecord -D hw:0,14 -f S16_LE -r 16000 -c 1 -d 5 /tmp/hs_wov.wav`.

---

## Further Reading
- [Developer Integration Guide: Custom WOV & ECNS Algorithms](../../../doc/developer_guides/wov_ecns_integration_guide.md)

