# Developer Integration Guide: Custom WOV & ECNS Algorithms

This guide provides step-by-step instructions, architectural blueprints, process flowcharts, and configuration details for developers integrating custom **Echo Cancellation & Noise Suppression (ECNS)** and **Wake-on-Voice (WOV)** algorithms into the Sound Open Firmware (SOF) multi-slot audio pipeline.

The reference topology establishes a unified 4-channel native 16 kHz architecture supporting concurrent multi-keyword spotting and acoustic front-end processing across **Panther Lake (PTL)**, **Tiger Lake (TGL)**, and **Wildcat Lake (WCL)**.

---

## Table of Contents
1. [System Architecture Overview](#1-system-architecture-overview)
2. [Developing a Custom ECNS Module](#2-developing-a-custom-ecns-module)
3. [Developing a Custom WOV Keyword Spotter](#3-developing-a-custom-wov-keyword-spotter)
4. [WoV Arbiter & Multi-Slot Coordination](#4-wov-arbiter--multi-slot-coordination)
5. [Real-Time Volatile Scoring Controls (2 Hz)](#5-real-time-volatile-scoring-controls-2-hz)
6. [Topology2 Pipeline Configuration](#6-topology2-pipeline-configuration)
7. [Process Flows & State Machines](#7-process-flows--state-machines)
8. [Step-by-Step Integration Recipe](#8-step-by-step-integration-recipe)
9. [Common Pitfalls & Troubleshooting](#9-common-pitfalls--troubleshooting)

---

## 1. System Architecture Overview

The pipeline separates real-time audio ingest into two primary scheduling domains:
- **Low-Latency (LL) Domain (1 ms period)**: Ingests 2/4-channel 16 kHz and 48 kHz microphone data from the DMIC DAIs and the speaker echo reference (48 kHz, converted to 16 kHz stereo by `src.120.1`) without frame jitter.
- **Data Processing (DP) Domain (20 ms period)**: Runs block-based processing for acoustic echo cancellation, noise suppression, circular buffering, feature extraction, and neural network keyword classification.

```mermaid
flowchart TD
    %% Hardware PDM Interfaces
    subgraph PDM_INTERFACES ["Digital Microphone Interfaces (2 Separate PDM DAIs)"]
        direction LR
        PDM_48K["<b>PDM Interface: dmic01</b><br/>DAI Index: 0 (PDM0)<br/>Rate: <b>48 kHz</b> · 2/4ch · 32-bit"]
        PDM_16K["<b>PDM Interface: dmic16k</b><br/>DAI Index: 1 (PDM1)<br/>Rate: <b>16 kHz</b> · 2/4ch · 32-bit"]
    end

    %% Pipeline 101: 48 kHz regular DMIC0 DAI capture (SoundWire IDs)
    subgraph P101 ["Pipeline 101: 48 kHz DMIC0 DAI Ingest (Core 0, LL 1ms)"]
        DAI_48K["dai-copier.DMIC.dmic01.capture<br/>(48 kHz · 2/4ch · S32_LE)"]
        GAIN_101["gain.101.1<br/>(Dmic0 Capture Volume / Switch)"]
        IIR_101["eqiir.101.1<br/>(DMIC0_DAI_EQIIR, 40 Hz high-pass)"]
        MCP_101["module-copier.101.2"]
        DAI_48K --> GAIN_101 --> IIR_101 --> MCP_101
    end

    %% Pipeline 119: 16 kHz DAI Capture
    subgraph P119 ["Pipeline 119: 16 kHz DAI Ingest (Core 0, LL 1ms)"]
        DAI_16K["dai-copier.DMIC.dmic16k.capture<br/>(16 kHz · 2/4ch · S32_LE)"]
    end

    PDM_48K --> DAI_48K
    PDM_16K --> DAI_16K

    %% Pipeline 120: Speaker echo reference
    SPK["Speaker playback<br/>(module-copier + loopback DAI,<br/>per platform)"]
    subgraph P120 ["Pipeline 120: Speaker Echo Reference (Core 0, LL 1ms)"]
        SIGGEN["siggen.120.1<br/>(48 kHz · 2ch · passthrough,<br/>silence when speaker is idle)"]
        SRC_120["src.120.1<br/>(48 kHz → 16 kHz · 2ch · S32_LE)"]
        SIGGEN --> SRC_120
    end
    SPK --> SIGGEN

    %% Pipeline 115: ECNS DP Pipeline
    subgraph P115 ["Pipeline 115: ECNS Processing (Core 0, DP 20ms, lp_mode 1)"]
        ECNS["ecns.115.1 (Custom ECNS DP Module · 20ms Period)<br/>━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━<br/><b>Pin 0 In:</b> 16 kHz mic, 2/4ch s32 (IBS 2560 bytes)<br/><b>Pin 1 In:</b> 16 kHz echo ref, 2ch s32 (IBS 2560 bytes)<br/>─────────────────────────────────<br/><b>Processing (reference ecns.c):</b><br/>• Mic channel enum: average or MicN<br/>• 100 Hz HPF (boost +30 dB / 0 dB)<br/>• AEC switch: subtract (L+R)/2 of ref<br/>• NS switch: no function yet<br/>─────────────────────────────────<br/><b>Pin 0 Out:</b> 16 kHz, 1ch mono (OBS 640 bytes)"]
    end

    DAI_16K -->|"Pin 0 In (16k mic)"| ECNS
    SRC_120 -->|"Pin 1 In (16k echo ref)"| ECNS

    %% Pipeline 100: Host Capture PCM 10 (regular DMIC0, SoundWire IDs)
    subgraph P100 ["Pipeline 100: DMIC0 Host Capture (Core 0, LL 1ms)"]
        HOST_10["host-copier.10.capture<br/>(PCM 10 'DMIC Raw': hw:0,10)<br/>2/4ch 48 kHz DMIC, no ECNS"]
    end

    MCP_101 --> HOST_10

    %% Pipeline 116: KPB History Buffer
    subgraph P116 ["Pipeline 116: KPB History Buffer (Core 0, LL 1ms)"]
        KPB["kpb.116.1<br/>(2.0s mono circular history = 64 KB)"]
    end

    %% Pipeline 118: Detector fanout
    subgraph P118 ["Pipeline 118: Detector Fanout (Core 0, LL 1ms)"]
        FANOUT["module-copier.118.1<br/>(Fan-out)"]
    end
    KPB -->|"Pin 0 (Live Feed)"| FANOUT

    ECNS -->|"Pin 0 Out (Mono 16k)"| KPB

    %% WOV Keyword Spotter Slots
    subgraph WOV_Slots ["Pipelines 111–113: Concurrent WOV Keyword Spotters (Core 0, DP 10ms)"]
        subgraph Slot0 ["Slot 0 (Pipeline 111)"]
            M1["module-copier.111.1"] --> FE0["Feature Extract<br>(MFCC)"] --> WOV0["WOV Engine 0<br>(e.g. Strawberry)"]
        end
        subgraph Slot1 ["Slot 1 (Pipeline 112)"]
            M2["module-copier.112.1"] --> FE1["Feature Extract<br>(MFCC)"] --> WOV1["WOV Engine 1<br>(e.g. Hi Intel)"]
        end
        subgraph Slot2 ["Slot 2 (Pipeline 113)"]
            M3["module-copier.113.1"] --> FE2["Feature Extract<br>(MFCC)"] --> WOV2["WOV Engine 2<br>(e.g. Orange)"]
        end
    end

    FANOUT --> M1
    FANOUT --> M2
    FANOUT --> M3

    %% Pipeline 114: WOV Arbiter & Host Capture PCM 12
    subgraph P114 ["Pipeline 114: WOV Arbiter & Capture (Core 0, LL 1ms)"]
        ARB["wov-arbiter.114.1<br/>(Priority / First-Detect Selector)"]
        HOST_12["host-copier.12.capture<br/>(PCM 12: hw:0,12)<br/>1ch Pre-roll + Live Keyword Speech<br/>capture_compatible_d0i3: true"]
        ARB --> HOST_12
    end

    KPB -->|"Pin 1 (Host Sink Drain)"| ARB
    WOV0 -->|"Pin 1 (Trigger 0)"| ARB
    WOV1 -->|"Pin 2 (Trigger 1)"| ARB
    WOV2 -->|"Pin 3 (Trigger 2)"| ARB

    style ECNS fill:#1b4f72,stroke:#5dade2,color:#fff
    style KPB fill:#1c4966,stroke:#5dade2,color:#fff
    style ARB fill:#4a235a,stroke:#bb8fce,color:#fff
    style HOST_10 fill:#1e8449,stroke:#58d68d,color:#fff
    style HOST_12 fill:#2d5a27,stroke:#58d68d,color:#fff
```

---

## 2. Developing a Custom ECNS Module

An ECNS component acts as a two-source Data Processing (DP) block. It consumes the interleaved multi-channel microphone stream and the speaker echo reference on two input pins and emits cleaned mono audio on a single output pin. The reference implementation `src/audio/ecns/ecns.c` averages the microphone channels to mono (or takes the single channel selected with the `mic channel` enum), applies a 100 Hz high-pass IIR (`boost` switch selects +30 dB or 0 dB), and when the `AEC switch` is on subtracts the `(L+R)/2` average of the echo reference after the IIR. The `NS switch` has no function yet. The echo reference is always consumed, also when the AEC switch is off.

### Audio Data Interface Contract
- **Input Pin 0 (Source 0 — Microphones)**:
  - Channels: 2 or 4 near-end microphones (`DMIC_WOV_DAI_CHANNELS`), from `dai-copier.DMIC.dmic16k.capture`.
  - Rate: 16,000 Hz.
  - Sample Format: `S32_LE` in the topology (the firmware also accepts `S16_LE`).
  - Frame Period: 20 ms (`ibs` 2,560 bytes).
- **Input Pin 1 (Source 1 — Speaker Echo Reference)**:
  - Channels: 2 (far-end playback, from `src.120.1`).
  - Rate: 16,000 Hz.
  - Sample Format: `S32_LE`.
  - Frame Period: 20 ms (320 frames = 2,560 bytes).
- **Output Pin 0 (Sink 0 — Clean Voice to KPB / WOV)**:
  - Channels: 1 (Mono echo-canceled speech).
  - Rate: 16,000 Hz.
  - Sample Format: `S16_LE` in the topology; the reference firmware follows the KPB input bit depth (16 or 32).
  - Frame Period: 20 ms (320 samples = 640 bytes at 16-bit).

There is no second output pin; the 48 kHz host capture is the regular DMIC0 capture of `dmic-generic.conf` from `dmic01` (SoundWire: `gain.101.1` -> `eqiir.101.1` -> PCM 10, HDA generic: PCM 6, ADL rt5682: PCM 99).

### C Implementation Skeleton (`src/audio/ecns/custom_ecns.c`)

```c
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/buffer.h>

#define ECNS_MIC_BYTES  2560 // 20 ms @ 16 kHz 4ch s16 or 2ch s32
#define ECNS_REF_BYTES  2560 // 20 ms @ 16 kHz 2ch s32
#define ECNS_OUT_BYTES  640  // 20 ms @ 16 kHz 1ch s16

struct custom_ecns_data {
    uint32_t period_bytes_mic;  // Pin 0 in: microphones
    uint32_t period_bytes_ref;  // Pin 1 in: speaker echo reference
    uint32_t period_bytes_out;  // Pin 0 out: mono clean
    void *algo_state;           // Vendor algorithm state handle
};

static int custom_ecns_init(struct processing_module *mod)
{
    struct custom_ecns_data *cd = mod_zalloc(mod, sizeof(*cd));
    if (!cd)
        return -ENOMEM;

    mod->priv.private = cd;
    // Initialize vendor AEC/NR algorithm instance
    return 0;
}

static int custom_ecns_process(struct processing_module *mod,
                               struct sof_source **sources, int num_of_sources,
                               struct sof_sink **sinks, int num_of_sinks)
{
    struct custom_ecns_data *cd = module_get_private_data(mod);
    const void *mic_buf;
    const void *ref_buf = NULL;
    void *out_buf;
    int ret;

    // 1. Check microphone input availability (Pin 0). A real module should
    //    map sources to pins by the IPC4 queue ID instead of the array index.
    if (source_get_data_available(sources[0]) < ECNS_MIC_BYTES)
        return 0;

    // 2. Check output space on Pin 0 (KPB path)
    if (sink_get_free_size(sinks[0]) < ECNS_OUT_BYTES)
        return 0;

    // 3. Acquire buffer pointers. The echo reference (Pin 1) is consumed
    //    when available; process without it if it has not arrived.
    source_get_data(sources[0], ECNS_MIC_BYTES, &mic_buf, NULL, NULL);
    if (num_of_sources > 1 &&
        source_get_data_available(sources[1]) >= ECNS_REF_BYTES)
        source_get_data(sources[1], ECNS_REF_BYTES, &ref_buf, NULL, NULL);
    sink_get_buffer(sinks[0], ECNS_OUT_BYTES, &out_buf, NULL, NULL);

    // 4. Run Vendor DSP AEC + NR Algorithm
    // - Consumes mic_buf (2 or 4ch microphones)
    // - Consumes ref_buf (2ch speaker echo reference, may be NULL)
    // - Writes 1ch mono clean speech to out_buf
    vendor_aec_nr_process(cd->algo_state, mic_buf, ref_buf, out_buf, 320);

    // 5. Commit & release buffers
    source_release_data(sources[0], ECNS_MIC_BYTES);
    if (ref_buf)
        source_release_data(sources[1], ECNS_REF_BYTES);
    sink_commit_buffer(sinks[0], ECNS_OUT_BYTES);

    return 0;
}
```

### Module Registration & UUID
Declare the module interface and assign a unique GUID registered in `uuid-registry.txt`:
```c
static const struct module_interface custom_ecns_interface = {
    .init = custom_ecns_init,
    .prepare = custom_ecns_prepare,
    .process = custom_ecns_process,
    .set_configuration = custom_ecns_set_config,
    .get_configuration = custom_ecns_get_config,
    .reset = custom_ecns_reset,
    .free = custom_ecns_free
};

DECLARE_MODULE_ADAPTER(custom_ecns_interface, custom_ecns_uuid, custom_ecns_tr);
SOF_MODULE_INIT(custom_ecns, sys_comp_module_custom_ecns_interface_init);
```

---

## 3. Developing a Custom WOV Keyword Spotter

Each keyword spotter pipeline occupies a dedicated dynamic pipeline slot (`custom-capture` in Topology2).

### Architectural Requirements
1. **Multi-Instance Isolation**: Support concurrent instantiation (Slot 0, Slot 1, Slot 2) within a single codebase or distinct modules without sharing global state or conflicting static memory.
2. **KPB Draining Notification**: When a keyword threshold is breached, signal the Key Phrase Buffer to begin draining pre-roll audio to host.
3. **Arbiter Detection Notification**: Notify the WOV Arbiter which slot triggered.
4. **Pause / Resume Handling**: Listen to arbiter control broadcasts so non-triggered slots pause inference and release compute cycles.

### Notification Mechanics

```c
#include <sof/audio/kpb.h>
#include <sof/audio/wov_arbiter/wov_arbiter.h>

static void custom_wov_trigger(struct processing_module *mod, uint8_t slot_id)
{
    struct comp_dev *dev = mod->dev;
    struct custom_wov_data *cd = module_get_private_data(mod);

    comp_info(dev, "Keyword detected on slot %u!", slot_id);

    // 1. Notify KPB to start history buffer draining (pre-roll)
    cd->client_data.r_ptr = NULL;
    cd->client_data.sink = NULL;
    cd->client_data.id = slot_id;
    cd->client_data.drain_req = 2000; // 2000 ms pre-roll
    cd->event_data.event_id = KPB_EVENT_BEGIN_DRAINING;
    cd->event_data.client_data = &cd->client_data;

    notifier_event(dev, NOTIFIER_ID_KPB_CLIENT_EVT,
                   NOTIFIER_TARGET_CORE_ALL_MASK,
                   &cd->event_data, sizeof(cd->event_data));

    // 2. Notify WOV Arbiter of detection
    struct wov_detect_notif detect_payload = {
        .slot_id = slot_id
    };
    notifier_event(dev, NOTIFIER_ID_WOV_DETECT,
                   NOTIFIER_TARGET_CORE_ALL_MASK,
                   &detect_payload, sizeof(detect_payload));
}
```

### Arbiter Control Callback (Pause / Resume)
Register a notifier callback during `prepare()`:
```c
static void on_wov_ctrl(void *arg, enum notify_id id, void *data)
{
    struct comp_dev *dev = arg;
    struct processing_module *mod = comp_mod(dev);
    struct custom_wov_data *cd = module_get_private_data(mod);
    const struct wov_ctrl_notif *n = data;

    if (n->cmd == WOV_ARB_CMD_PAUSE && n->slot_id != cd->slot_id) {
        comp_info(dev, "Slot %u paused: slot %u is active", cd->slot_id, n->slot_id);
        cd->paused = true;
    } else if (n->cmd == WOV_ARB_CMD_RESUME) {
        comp_info(dev, "Slot %u resumed by arbiter", cd->slot_id);
        cd->paused = false;
        vendor_wov_reset(cd->algo_state);
    }
}
```

---

## 4. WoV Arbiter & Multi-Slot Coordination

The `wov_arbiter` component synchronizes multi-slot pipelines and multiplexes detected streams onto `host-copier.12`:

```mermaid
sequenceDiagram
    autonumber
    participant WOV0 as WOV Slot 0 (Strawberry)
    participant WOV1 as WOV Slot 1 (Hi Intel)
    participant KPB as KPB (Pipeline 116)
    participant ARB as WOV Arbiter (Pipeline 114)
    participant HOST as Linux ALSA Driver (PCM 12)

    Note over WOV0,WOV1: Both slots actively running 20ms inference
    WOV0->>WOV0: Keyword Match (Prob >= 0.70)
    WOV0->>KPB: notifier_event(NOTIFIER_ID_KPB_CLIENT_EVT, BEGIN_DRAINING)
    WOV0->>ARB: notifier_event(NOTIFIER_ID_WOV_DETECT, slot=0)
    
    rect rgb(40, 20, 60)
        Note over ARB: Arbiter latches Slot 0 as active winner
        ARB->>WOV1: notifier_event(NOTIFIER_ID_WOV_CTRL, PAUSE slot=0)
        ARB->>HOST: SOF_IPC4_MODULE_NOTIFICATION (DMIC Phrase Detect ID = 'Slot 1')
    end
    
    Note over WOV1: Slot 1 pauses inference (saves DSP MIPS)
    KPB->>ARB: Drains 2.0s Pre-roll audio history
    ARB->>HOST: Streams audio to /dev/snd/pcmC0D12c
    Note over ARB,HOST: Keyword phrase + post-trigger command audio streaming
```

### Arbiter Control State Enum
The arbiter exposes the `DMIC Phrase Detect ID` (and `Jack In Phrase Detect ID` for headset WoV) ALSA mixer enum control:
- `Item #0 'Listening'`: All slots actively scanning input.
- `Item #1 'Slot 1'`: Slot 0 detected keyword; sibling slots paused.
- `Item #2 'Slot 2'`: Slot 1 detected keyword; sibling slots paused.
- `Item #3 'Slot 3'`: Slot 2 detected keyword; sibling slots paused.

---

## 5. Real-Time Volatile Scoring Controls (2 Hz)

To give users and applications immediate feedback on speech confidence and keyword proximity, each WOV slot exposes a volatile read-only scoring kcontrol (e.g. `MWW 101 Score`, `MWW 102 Score`, `MWW 103 Score`).

### Kernel Dynamic Widget Constraint
> [!IMPORTANT]
> In the Linux SOF driver (`sound/soc/sof/topology.c`), dynamic widgets (`custom-capture` pipelines) are **prohibited** from including `SNDRV_CTL_ELEM_ACCESS_VOLATILE` in their topology configuration:
> ```c
> if (pipe_widget->dynamic_pipeline_widget) {
>     if (scontrol->access & SNDRV_CTL_ELEM_ACCESS_VOLATILE)
>         return -EINVAL; // Rejects topology loading!
> }
> ```
> **Solution**: Omit `volatile` from the topology `access []` list. Real-time updates are pushed asynchronously by the DSP firmware using `SOF_IPC4_MODULE_NOTIFICATION` with `SOF_IPC4_ENUM_CONTROL_PARAM_ID`. The kernel notification handler updates `cdata->chanv[channel].value` directly in the local cache and triggers `snd_ctl_notify()`.

### Discrete Enum Mapping (0% to 100%)
ALSA topology limits enum text items to `SND_SOC_TPLG_NUM_TEXTS = 16`. Eleven 10% interval items fit well within this bound:
```
Item #0: 0%  | Item #1: 10% | Item #2: 20% | Item #3: 30% | Item #4: 40%
Item #5: 50% | Item #6: 60% | Item #7: 70% | Item #8: 80% | Item #9: 90% | Item #10: 100%
```

### Pushing Asynchronous 2 Hz Notifications from DSP

```c
#if CONFIG_IPC_MAJOR_4
static void custom_wov_notify_score(const struct comp_dev *dev, uint32_t score_idx)
{
    struct sof_ipc4_notify_module_data *msg_module_data;
    struct sof_ipc4_control_msg_payload *msg_payload;
    struct ipc_msg *msg;
    uint32_t data_size = sizeof(struct sof_ipc4_notify_module_data) +
                         sizeof(struct sof_ipc4_control_msg_payload) +
                         sizeof(struct sof_ipc4_ctrl_value_chan);
    struct ipc4_voice_cmd_notification notif;

    memset_s(&notif, sizeof(notif), 0, sizeof(notif));
    notif.primary.r.notif_type = SOF_IPC4_MODULE_NOTIFICATION;
    notif.primary.r.type = SOF_IPC4_GLB_NOTIFICATION;
    notif.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
    notif.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;

    msg = ipc_msg_w_ext_init(notif.primary.dat, 0, data_size);
    if (!msg)
        return;

    msg_module_data = (struct sof_ipc4_notify_module_data *)msg->tx_data;
    msg_module_data->instance_id = IPC4_INST_ID(dev->ipc_config.id);
    msg_module_data->module_id = IPC4_MOD_ID(dev->ipc_config.id);
    msg_module_data->event_id = SOF_IPC4_NOTIFY_MODULE_EVENTID_ALSA_MAGIC_VAL |
                                SOF_IPC4_ENUM_CONTROL_PARAM_ID;
    msg_module_data->event_data_size = sizeof(struct sof_ipc4_control_msg_payload) +
                                       sizeof(struct sof_ipc4_ctrl_value_chan);

    msg_payload = (struct sof_ipc4_control_msg_payload *)msg_module_data->event_data;
    msg_payload->id = 0; // Control index on this widget
    msg_payload->num_elems = 1;
    msg_payload->chanv[0].channel = 0;
    msg_payload->chanv[0].value = score_idx;

    ipc_msg_send(msg, NULL, false);
}
#endif
```

### Periodic Tracking in Process Loop (500 ms Windows)
```c
// Track peak probability across 50 hops (500 ms @ 10ms/hop = 2 Hz)
cd->score_hop_counter++;
if (cd->score_hop_counter >= 50) {
    cd->score_hop_counter = 0;
    uint32_t s_idx = (uint32_t)(cd->window_peak_prob * 10.0f + 0.5f);
    if (s_idx > 10) s_idx = 10;
    cd->current_score_idx = (uint8_t)s_idx;
    cd->window_peak_prob = 0.0f;

#if CONFIG_IPC_MAJOR_4
    if (cd->current_score_idx != cd->last_notified_score_idx || cd->current_score_idx > 0) {
        cd->last_notified_score_idx = cd->current_score_idx;
        custom_wov_notify_score(dev, cd->current_score_idx);
    }
#endif
}
```

---

## 6. Topology2 Pipeline Configuration

### Component Include: Scoring Enum Control (`score_enum.conf`)
Path: `tools/topology/topology2/include/components/mww/score_enum.conf`
```conf
Class.Control.enum."score_enum" {
    # Omitting 'volatile' access flag to satisfy Linux dynamic widget validation
    access [
        read
        tlv_read
    ]

    enum_texts [
        "0%"
        "10%"
        "20%"
        "30%"
        "40%"
        "50%"
        "60%"
        "70%"
        "80%"
        "90%"
        "100%"
    ]

    IncludeByKey.NUM_ELEMS {
        "1" {
            ControlEnumObject [
                {
                    channel "all"
                    texts [
                        $enum_texts
                    ]
                }
            ]
        }
    }
}
```

### Pipeline Instantiation in Manifest

> [!NOTE]
> The snippets below were taken from the standalone DMIC-only
> `dmic-wov-multi-ptl-4ch-manifest.conf`, which has been removed because ECNS
> requires a speaker echo reference input that a DMIC-only topology cannot
> provide. They are kept as an illustration of the pipeline structure. The
> current ECNS in `platform/intel/dmic-wov-multi.conf` (`ecns.115.1`) has two
> input pins (16 kHz microphones from `dmic16k` and the 16 kHz stereo speaker
> echo reference from `src.120.1`) and a single 16 kHz mono output pin to
> `kpb.116.1`, without mixin/mixout widgets.

#### 1. ECNS Pipeline (Pipeline 105)
```conf
Object.Pipeline.custom-processing."105" {
    index 105
    period 20000        # 20 ms
    time_domain "timer"
    core 0

    Object.Widget.mixout."1" {
        index 105
    }

    Object.Widget.ecns."1" {
        index 105
        uuid $ECNS_UUID
        num_input_pins 1
        num_output_pins 2
    }

    Object.Widget.mixin."1" { # Pin 0 Mono Clean
        index 105
    }

    Object.Widget.mixin."2" { # Pin 1 Stereo Clean
        index 105
    }
}
```

#### 2. Detection Slot Pipeline (Pipeline 101)
```conf
Object.Pipeline.custom-capture."101" {
    index 101
    period 20000
    core 0

    Object.Widget.mixout."1" {
        index 101
    }

    Object.Widget.mfcc."1" {
        index 101
    }

    Object.Widget.mww."1" {
        index 101
        Object.Control.enum."1" {
            name "MWW 101 Score"
            IncludeByKey.SCORE_ENUM {
                "score_enum" "include/components/mww/score_enum.conf"
            }
        }
    }
}
```

---

## 7. Process Flows & State Machines

### A. Lifecycle State Machine for Dynamic WOV Widgets
```mermaid
stateDiagram-v2
    [*] --> Off: Driver Boot / Card Instantiated
    Off --> Ready: PCM Open (/dev/snd/pcmC0D12c)
    Ready --> Active: arecord Started / DSP Stream Running

    state Active {
        [*] --> Buffering: Slices filling (0..2 hops)
        Buffering --> Inference: Slices full (3 hops = 30ms)
        Inference --> UpdateScore: Every 50 hops (500 ms / 2 Hz)
        UpdateScore --> PushNotification: Score > 0 or Changed
        PushNotification --> Inference
        Inference --> KeywordDetected: Confidence >= Threshold
    }

    KeywordDetected --> Paused: Arbiter signals other slots
    Paused --> Active: Arbiter resumes stream
    Active --> Off: PCM Closed / Pipeline Reset
```

### B. Scoring Engine Update Loop
```mermaid
flowchart TD
    Start([Audio Hop Arrives: 10 ms]) --> Extract[Compute / Ingest Feature Slice]
    Extract --> Infer[Run Keyword Inference Model]
    Infer --> ComparePeak{Inference Prob > Window Peak?}
    ComparePeak -- Yes --> SetPeak[Window Peak = Prob]
    ComparePeak -- No --> CheckHop
    SetPeak --> CheckHop{50 Hops Elapsed?<br>500 ms / 2 Hz}
    CheckHop -- No --> Finish([Wait Next Hop])
    CheckHop -- Yes --> MapIndex["Calculate Enum Index:<br>clamp(floor(Peak * 10 + 0.5), 0, 10)"]
    MapIndex --> HasChanged{Index != Last Notified<br>OR Index > 0?}
    HasChanged -- Yes --> SendIPC["Dispatch IPC4 Module Notification:<br>SOF_IPC4_ENUM_CONTROL_PARAM_ID"]
    HasChanged -- No --> ResetPeak
    SendIPC --> ResetPeak[Window Peak = 0.0<br>Hop Counter = 0]
    ResetPeak --> Finish
```

---

## 8. Step-by-Step Integration Recipe

Follow this step-by-step checklist to integrate your own algorithms:

### Step 1: Implement DSP Module C/C++ Code
1. Place source code under `src/audio/<your_algo>/`.
2. Implement standard `module_interface` (`init`, `prepare`, `process`, `set_configuration`, `get_configuration`, `reset`, `free`).
3. For ECNS, handle 2 or 4ch microphones in (Pin 0), 2ch speaker echo reference in (Pin 1), and 1ch out (Pin 0).
4. For WOV, implement multi-instance state isolation, 2 Hz scoring notification, and register for `NOTIFIER_ID_WOV_CTRL`.

### Step 2: Register Module UUID
1. Generate a UUID (e.g. `uuidgen`).
2. Add definition to `uuid-registry.txt` and `tools/rimage/config/<platform>.toml.h`.
3. In `CMakeLists.txt` and `Kconfig`, register the component with `sof_audio_module()`.

### Step 3: Define Topology Components
1. Create Topology2 component definitions in `tools/topology/topology2/include/components/<your_algo>.conf`.
2. Reference the scoring enum in `score_enum.conf` (ensuring no `volatile` access flag).
3. Connect widgets in the shared multi-slot WoV topology
   `tools/topology/topology2/platform/intel/dmic-wov-multi.conf`, used e.g. by the
   `cavs-sdw` `*-wov-multi` targets in `production/tplg-targets-ace3.cmake`.
   Wildcat Lake topologies are symlinks to the Panther Lake ones.
4. Route the speaker echo reference into `siggen.120.1` of the echo reference
   pipeline 120 (`siggen-src-capture`), which feeds ECNS input pin 1 through
   `src.120.1`. This is enabled with `SDW_SPK_ECHO_REF_ECNS=true` (SoundWire,
   together with `SDW_SPK_ECHO_REF=true` and `SDW_ECHO_REF_DAI=true`),
   `SPEAKER_ECHO_REF_ECNS=true` (ADL rt5682 SSP, needs `INCLUDE_ECHO_REF=true`)
   or `HDA_SPEAKER_ECHO_REF_ECNS=true` (HDA generic). A dai_out widget upstream
   of `siggen.120.1` (e.g. `alh-copier.Loopback_Virtual.25` or the SSP loopback
   capture DAI) starts the setup of pipeline 120 with the WoV capture; HDA has
   no such DAI and it is untested whether the kernel sets the pipeline up there.

### Step 4: Compile Topology & Firmware
1. Build the topology via its CMake target (see `tools/topology/topology2/README.md`
   for the direct `alsatplg -p` invocation):
   ```bash
   make -C build-tools topology2_prod_sof-ptl-rt721-4ch-wov-multi
   ```
2. Build firmware with Zephyr SDK GCC:
   ```bash
   export ZEPHYR_SDK_INSTALL_DIR=${ZEPHYR_SDK_INSTALL_DIR:-/path/to/zephyr-sdk}
   ./sof/scripts/xtensa-build-zephyr.py -z ptl
   ```

### Step 5: Deploy & Validate on Hardware
1. Copy firmware and topology to DUT:
   ```bash
   scp build-sof-staging/sof/intel/sof-ipc4/ptl/community/sof-ptl.ri root@<dut>:/lib/firmware/intel/sof-ipc4/ptl/community/
   scp build-tools/topology/topology2/production/sof-ptl-rt721-4ch-wov-multi.tplg root@<dut>:/lib/firmware/intel/sof-ipc4-tplg/
   ```
2. Reload driver:
   ```bash
   ssh root@<dut> 'rmmod snd_sof_pci_intel_ptl && modprobe snd_sof_pci_intel_ptl'
   ```
3. Verify controls:
   ```bash
   ssh root@<dut> 'amixer -c 0 controls | grep -E "Score|wov"'
   ```

---

## 9. Common Pitfalls & Troubleshooting

| Symptom / Error | Root Cause | Fix |
|---|---|---|
| `error: volatile control found for dynamic widget` (`-EINVAL` / `-22`) | Topology sets `access [ read volatile ]` on a dynamic widget control. | Remove `volatile` from `access []` in `score_enum.conf`. Rely on DSP IPC4 module notifications. |
| Topology compile fails with `-ENOMEM` on Enum | Enum texts exceed `SND_SOC_TPLG_NUM_TEXTS` (16 items). | Keep discrete score intervals $\le 16$ items (e.g. 11 items for 0%–100% in 10% steps). |
| Kernel drops scoring notification | Control parameter ID is not recognized by `sof_ipc4_control_notification`. | Use `SOF_IPC4_ENUM_CONTROL_PARAM_ID` (201) and format payload as `sof_ipc4_control_msg_payload`. |
| Second / third keyword slot crashes | Global static state collision across multiple instances. | Isolate state per instance inside an array (`g_instances[kMaxSlots]`) indexed by slot ID. |
| Score sticks at old value across stream runs | DSP reset does not push reset notification to kernel cache. | Call `custom_wov_notify_score(mod->dev, 0)` in `reset()` or stream startup. |
