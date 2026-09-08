# microWakeWord (MWW) Architecture & Multi-Slot WoV Integration

This directory provides the Sound Open Firmware (SOF) component wrapping [OHF-Voice's
microWakeWord](https://github.com/OHF-Voice/micro-wake-word) streaming
keyword-spotting model, integrated with MFCC feature extraction and the
multi-slot Key Phrase Buffer (KPB) Wake-on-Voice (WoV) arbitration infrastructure.

Unlike standard single-keyword detectors or 4-way softmax classifiers, microWakeWord
executes streaming inference over a stateful TensorFlow Lite Micro (TFLM) graph,
outputting a single sigmoid wake-word probability per keyword model.

In this architecture, **one unified codebase** supports **three concurrent keyword spotter instances**:
- **Slot 0** (Pipeline 101): `"strawberry"`
- **Slot 1** (Pipeline 102): `"banana"`
- **Slot 2** (Pipeline 103): `"orange"`

---

## Architecture & Data Flow

The multi-slot MWW topology is unified across **Tiger Lake (TGL)**, **Panther Lake (PTL)**, and **Wildcat Lake (WCL)**:

```mermaid
graph TD
    subgraph P100["Pipeline 100 — 4ch DMIC Capture  (Core 0, LL 1ms)"]
        DAI["DAI Copier (dmic01 / dmic16k)\n4ch · 16 kHz · S16_LE\nCh 0,1: Mics | Ch 2,3: Echo Ref"]
        MIX100["mixin 100.1\n(4ch pass-through)"]
        DAI --> MIX100
    end

    subgraph P105["Pipeline 105 — ECNS DP Processing  (Core 0, DP 20ms)"]
        MO105["mixout 105.1\n(4ch input)"]
        ECNS["ecns.105.1\n(ECNS DP Module, 20ms = 320 frames)\nCh 0,1: Mic | Ch 2,3: Echo Ref"]
        MIX105_1["mixin 105.1\nPin 0: Ch 0 Mono Clean"]
        MIX105_2["mixin 105.2\nPin 1: Ch 0,1 Stereo Clean"]
        MO105 --> ECNS
        ECNS -- "Pin 0 (Mono)" --> MIX105_1
        ECNS -- "Pin 1 (Stereo)" --> MIX105_2
    end

    subgraph P106["Pipeline 106 — KPB History Buffer  (Core 0, DP 20ms)"]
        MO106["mixout 106.1\n(1ch mono)"]
        KPB["kpb.106.1\n(2.0s mono history = 64 KB)\n16 kHz · 1ch · S16_LE"]
        MIX106["mixin 106.1\n(3-way fanout mixin)"]
        MO106 --> KPB --> MIX106
    end

    subgraph P101["Pipeline 101 — Slot 0: 'strawberry'  (Core 0, DP 10ms)"]
        MO101["mixout 101.1"]
        MFCC0["mfcc.101.1\n(Mel-40 10ms Compress)\nIBS: 320B | OBS: 184B"]
        MWW0["mww.101.1\n(microWakeWord)\nModel: 'strawberry'"]
        MO101 --> MFCC0 --> MWW0
    end

    subgraph P102["Pipeline 102 — Slot 1: 'banana'  (Core 0, DP 10ms)"]
        MO102["mixout 102.1"]
        MFCC1["mfcc.102.1\n(Mel-40 10ms Compress)\nIBS: 320B | OBS: 184B"]
        MWW1["mww.102.1\n(microWakeWord)\nModel: 'banana'"]
        MO102 --> MFCC1 --> MWW1
    end

    subgraph P103["Pipeline 103 — Slot 2: 'orange'  (Core 0, DP 10ms)"]
        MO103["mixout 103.1"]
        MFCC2["mfcc.103.1\n(Mel-40 10ms Compress)\nIBS: 320B | OBS: 184B"]
        MWW2["mww.103.1\n(microWakeWord)\nModel: 'orange'"]
        MO103 --> MFCC2 --> MWW2
    end

    subgraph P104["Pipeline 104 — WOV Host PCM Capture  (Core 0, LL 1ms)"]
        ARB["wov-arbiter.104.1\n(3 input pins, 1 output pin\n1ch mono 16 kHz)"]
        HC11["host-copier.11\n(hw:0,11 · PCM 11)\n1ch · 16 kHz · S16_LE / S32_LE"]
        ARB --> HC11
    end

    subgraph P107["Pipeline 107 — ECNS Host PCM Capture  (Core 0, LL 1ms)"]
        MO107["mixout 107.1\n(2ch stereo)"]
        HC10["host-copier.10\n(hw:0,10 · PCM 10)\n2ch · 16 kHz · S16_LE / S32_LE"]
        MO107 --> HC10
    end

    MIX100 --> MO105
    MIX105_1 --> MO106
    MIX105_2 --> MO107
    MIX106 --> MO101
    MIX106 --> MO102
    MIX106 --> MO103

    MWW0 --> ARB
    MWW1 --> ARB
    MWW2 --> ARB

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
    style HC11 fill:#2d5a27,stroke:#555,color:#fff
    style HC10 fill:#1e8449,stroke:#555,color:#fff
```

---

## Cross-Platform Manifest Targets

| Target Platform | Manifest File | Output Topology Binary | DMIC Driver Version |
|---|---|---|---|
| **Panther Lake (PTL / ACE 3.0)** | [`dmic-wov-multi-ptl-4ch-manifest.conf`](file:///home/lrg/work/sof-tgl/sof-wov/tools/topology/topology2/dmic-wov-multi-ptl-4ch-manifest.conf) | `sof-ptl-dmic-wov-multi-4ch.tplg` | 5 |
| **Tiger Lake (TGL / CAVS 2.5)** | [`dmic-wov-multi-4ch-manifest.conf`](file:///home/lrg/work/sof-tgl/sof-wov/tools/topology/topology2/dmic-wov-multi-4ch-manifest.conf) | `sof-tgl-dmic-wov-multi-4ch.tplg` | 1 |
| **Wildcat Lake (WCL / ACE 3.0)** | [`dmic-wov-multi-wcl-4ch-manifest.conf`](file:///home/lrg/work/sof-tgl/sof-wov/tools/topology/topology2/dmic-wov-multi-wcl-4ch-manifest.conf) | `sof-wcl-dmic-wov-multi-4ch.tplg` | 5 |

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
Default built-in models are bound automatically by slot/pipeline index:
- **Slot 0** (`pipeline_id == 101`): `mww_model_data_strawberry`
- **Slot 1** (`pipeline_id == 102`): `mww_model_data_banana`
- **Slot 2** (`pipeline_id == 103`): `mww_model_data_orange`

Each instance can also be dynamically customized via SOF IPC4 `LARGE_CONFIG_SET` / bytes control or LLEXT model loading.

---

## Notifier Inter-Module Signaling Contract

The MWW detector participates in SOF's synchronous intra-DSP publish/subscribe notifier bus:

1. **`NOTIFIER_ID_WOV_DETECT`**: Fired by `mww` to `wov_arbiter` on keyword detection (`probability >= MWW_DETECT_THRESHOLD`). Passes payload `{ .slot_id = cd->wov_slot_id }`.
2. **`NOTIFIER_ID_KPB_CLIENT_EVT`**: Fired by `mww` to `kpb` to request immediate draining of the 2.0 s pre-roll history buffer.
3. **`NOTIFIER_ID_WOV_CTRL`**: Received by `mww` from `wov_arbiter`.
   - `WOV_ARB_CMD_PAUSE`: Pauses inference while another slot's keyword audio is streaming to the host.
   - `WOV_ARB_CMD_RESUME`: Re-arms the detector when the host closes or resets the capture stream.

---

## ALSA Kcontrols & Userspace Verification

### 1. Active Slot Status (`wov_active_slot`)
The arbiter exposes a volatile read-only ALSA enum kcontrol:
```bash
amixer -c 0 cget name='wov_active_slot'
# Values: 0 = Listening, 1 = Slot 1 ("strawberry"), 2 = Slot 2 ("banana"), 3 = Slot 3 ("orange")
```

### 2. Audio Capture Endpoints
- **Stereo ECNS Stream** (PCM 10): `arecord -D hw:0,10 -f S16_LE -r 16000 -c 2 -d 5 /tmp/ecns.wav`
- **Mono WOV Capture Stream** (PCM 11): `arecord -D hw:0,11 -f S16_LE -r 16000 -c 1 -d 5 /tmp/wov.wav` (supports both `S16_LE` and `S32_LE`).

---

## Further Reading
- [Developer Integration Guide: Custom WOV & ECNS Algorithms](../../../doc/developer_guides/wov_ecns_integration_guide.md)

