# IPC3 Architecture

This directory houses the Version 3 Inter-Processor Communication handling components. IPC3 is the older, legacy framework structure used extensively across initial Sound Open Firmware releases before the transition to IPC4 compound pipeline commands.

## Overview

The IPC3 architecture treats streaming, DAI configurations, and pipeline management as distinct scalar events. Messages arrive containing a specific `sof_ipc_cmd_hdr` denoting the "Global Message Type" (e.g., Stream, DAI, Trace, PM) and the targeted command within that type.

## Command Structure and Routing

Every message received is placed into an Rx buffer and initially routed to `ipc_cmd()`. Based on the `cmd` inside the `sof_ipc_cmd_hdr`, it delegates to one of the handler subsystems:

* `ipc_glb_stream_message`: Stream/Pipeline configuration and states
* `ipc_glb_dai_message`: DAI parameters and formats
* `ipc_glb_pm_message`: Power Management operations

```mermaid
graph TD
    Mailbox[IPC Mailbox Interrupt] --> Valid[mailbox_validate]
    Valid --> Disp[IPC Core Dispatcher]

    Disp -->|Global Type 1| StreamMsg[ipc_glb_stream_message]
    Disp -->|Global Type 2| DAIMsg[ipc_glb_dai_message]
    Disp -->|Global Type 3| PMMsg[ipc_glb_pm_message]
    Disp -->|Global Type ...| TraceMsg[ipc_glb_trace_message]

    subgraph Stream Commands
        StreamMsg --> StreamAlloc[ipc_stream_pcm_params]
        StreamMsg --> StreamTrig[ipc_stream_trigger]
        StreamMsg --> StreamFree[ipc_stream_pcm_free]
        StreamMsg --> StreamPos[ipc_stream_position]
    end

    subgraph DAI Commands
        DAIMsg --> DAIConf[ipc_msg_dai_config]
    end

    subgraph PM Commands
        PMMsg --> PMCore[ipc_pm_core_enable]
        PMMsg --> PMContext[ipc_pm_context_save / restore]
    end
```

## Processing Flows

### Stream Triggering (`ipc_stream_trigger`)

Triggering is strictly hierarchical via IPC3. It expects pipelines built and components fully parsed prior to active streaming commands.

1. **Validation**: The IPC fetches the host component ID.
2. **Device Lookup**: It searches the components list (`ipc_get_comp_dev`) for the PCM device matching the pipeline.
3. **Execution**: If valid, the pipeline graph is crawled recursively and its state altered via `pipeline_trigger`.

```mermaid
sequenceDiagram
    participant Host
    participant IPC3 as IPC3 Handler (ipc_stream_trigger)
    participant Pipe as Pipeline Framework
    participant Comp as Connected Component

    Host->>IPC3: Send SOF_IPC_STREAM_TRIG_START
    activate IPC3
    IPC3->>IPC3: ipc_get_comp_dev(stream_id)
    IPC3->>Pipe: pipeline_trigger(COMP_TRIGGER_START)
    activate Pipe
    Pipe->>Comp: pipeline_for_each_comp(COMP_TRIGGER_START)
    Comp-->>Pipe: Success (Component ACTIVE)
    Pipe-->>IPC3: Return Status
    deactivate Pipe

    alt If Success
        IPC3-->>Host: Acknowledge Success Header
    else If Error
        IPC3-->>Host: Acknowledge Error Header (EINVAL / EIO)
    end
    deactivate IPC3
```

### DAI Configuration (`ipc_msg_dai_config`)

DAI (Digital Audio Interface) configuration involves setting up physical I2S, ALH, SSP, or HDA parameters.

1. **Format Unpacking**: Converts the `sof_ipc_dai_config` payload sent from the ALSA driver into an internal DSP structure `ipc_config_dai`.
2. **Device Selection**: Identifies the exact DAI interface and finds its tracking device ID via `dai_get`.
3. **Hardware Config**: Applies the unpacked settings directly to the hardware via the specific DAI driver's `set_config` function.

```mermaid
sequenceDiagram
    participant Host
    participant IPC3 as IPC3 Handler (ipc_msg_dai_config)
    participant DAIDev as DAI Framework (dai_get)
    participant HWDriver as HW Specific Driver (e.g. SSP)

    Host->>IPC3: Send SOF_IPC_DAI_CONFIG (e.g., SSP1, I2S Format)
    activate IPC3

    IPC3->>IPC3: build_dai_config()
    IPC3->>DAIDev: dai_get(type, index)
    DAIDev-->>IPC3: pointer to dai instance

    IPC3->>HWDriver: dai_set_config()
    activate HWDriver
    HWDriver-->>HWDriver: configures registers
    HWDriver-->>IPC3: hardware configured
    deactivate HWDriver

    IPC3-->>Host: Acknowledged Setting
    deactivate IPC3
```

## Mailbox and Validation (`mailbox_validate`)

All commands passing through this layer enforce rigid payload boundaries. `mailbox_validate()` reads the first word directly from the mailbox memory, identifying the command type before parsing parameters out of shared RAM to prevent host/DSP mismatches from cascading.
