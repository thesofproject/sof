# Audio Processing Modules (`src/module`)

The `src/module` directory and the `src/include/module` headers define the Sound Open Firmware (SOF) modern Audio Processing Module API. This architecture abstracts the underlying OS and pipeline scheduler implementations from the actual audio signal processing logic, allowing modules to be written once and deployed either as statically linked core components or as dynamically loadable Zephyr EXT (LLEXT) modules.

## Architecture Overview

The SOF module architecture is built around three core concepts:

1. **`struct module_interface`**: A standardized set of operations (`init`, `prepare`, `process`, `reset`, `free`, `set_configuration`) that every audio processing module implements.
2. **`struct processing_module`**: The runtime instantiated state of a module. It holds metadata, configuration objects, memory allocations (`module_resources`), and references to interconnected streams.
3. **Module Adapter (`src/audio/module_adapter`)**: The system glue layer. It masquerades as a legacy pipeline `comp_dev` to the SOF schedulers, but acts as a sandbox container for the `processing_module`. It intercepts IPC commands, manages the module's state machine, manages inputs/outputs, and safely calls the `module_interface` operations.

```mermaid
graph TD
    subgraph SOF Core Pipeline Scheduler
        P[Pipeline Scheduler <br> LL/DP Domains]
    end

    subgraph Module Adapter System Layer
        MA[Module Adapter `comp_dev`]
        IPC[IPC Command Dispatch]
        MEM[Memory Resource Manager]
    end

    subgraph Standardized Module Framework API
        INF[`module_interface` Ops]
        SRC[Source API <br> `source_get_data`]
        SNK[Sink API <br> `sink_get_buffer`]
    end

    subgraph Custom Audio Modules
        VOL[Volume]
        EQ[EQ FIR/IIR]
        CUSTOM[Loadable 3rd Party <br> Zephyr LLEXT]
    end

    P <-->|Execute| MA
    IPC -->|Config/Triggers| MA

    MA -->|Invoke| INF
    MA -->|Manage| MEM

    INF --> VOL
    INF --> EQ
    INF --> CUSTOM

    VOL -->|Read| SRC
    VOL -->|Write| SNK
    EQ -->|Read| SRC
    EQ -->|Write| SNK
```

## Module State Machine

Every processing module is strictly governed by a uniform runtime state machine managed by the `module_adapter`. Modules must adhere to the transitions defined by `enum module_state`:

1. `MODULE_DISABLED`: The module is loaded but uninitialized, or has been freed. No memory is allocated.
2. `MODULE_INITIALIZED`: After a successful `.init()` call. The module parses its IPC configuration and allocates necessary local resources (delay lines, coefficient tables).
3. `MODULE_IDLE`: After a successful `.prepare()` call. Audio stream formats are fully negotiated and agreed upon (Stream params, channels, rate).
4. `MODULE_PROCESSING`: When the pipeline triggers a `START` command. The `.process()` callback is actively handling buffers.

```mermaid
stateDiagram-v2
    [*] --> MODULE_DISABLED: Module Created

    MODULE_DISABLED --> MODULE_INITIALIZED: .init() / IPC NEW
    MODULE_INITIALIZED --> MODULE_DISABLED: .free() / IPC FREE

    MODULE_INITIALIZED --> MODULE_IDLE: .prepare() / Pipeline Setup
    MODULE_IDLE --> MODULE_INITIALIZED: .reset() / Pipeline Reset

    MODULE_IDLE --> MODULE_PROCESSING: .trigger(START) / IPC START
    MODULE_PROCESSING --> MODULE_IDLE: .trigger(STOP/PAUSE) / IPC STOP
```

## Data Flows and Buffer Management

Modules do not directly manipulate underlying DMA, ALSA, or Zephyr `comp_buffer` pointers. Instead, they interact via the decoupled **Source and Sink APIs**. This allows the adapter to seamlessly feed data from varying topological sources without changing module code.

The flow operates primarily in a "get -> manipulate -> commit/release" pattern:

```mermaid
sequenceDiagram
    participant Adapter as Module Adapter
    participant Mod as Processing Module (.process)
    participant Src as Source API (Input)
    participant Snk as Sink API (Output)

    Adapter->>Mod: Process Trigger (sources[], sinks[])

    Mod->>Src: source_get_data(req_size)
    Src-->>Mod: Provides read_ptr, available_bytes

    Mod->>Snk: sink_get_buffer(req_size)
    Snk-->>Mod: Provides write_ptr, free_bytes

    note over Mod: Execute DSP Algorithm <br> (Read from read_ptr -> Write to write_ptr)

    Mod->>Src: source_release_data(consumed_bytes)
    Mod->>Snk: sink_commit_buffer(produced_bytes)

    Mod-->>Adapter: Return Status
```

### Source API (`src/module/audio/source_api.c`)
- modules request data by calling `source_get_data_s16` or `s32`. This establishes an active read lock.
- Once done, the module must call `source_release_data()` releasing only the frames actually consumed.

### Sink API (`src/module/audio/sink_api.c`)
- modules request destination buffers by calling `sink_get_buffer_s16` or `s32`.
- After processing into the provided memory array, the module marks the memory as valid by calling `sink_commit_buffer()` for the exact number of frames successfully written.
