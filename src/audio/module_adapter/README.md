# Module Adapter Architecture

This directory contains the Module Adapter.

## Overview

The Module Adapter is a crucial piece of the IPC4 pipeline, allowing the core SOF graph mechanism to interact with 3rd party processing modules (like Windows APOs or customized vendor DSP engines) through a generic wrapper format.

## Creation and Teardown Flow

The Module Adapter wraps an external DSP processing component and translates standard SOF pipeline calls (like `comp_new`, `comp_free`, `comp_trigger`) into an interface the external module natively understands (`ops->init`, `ops->free`, `ops->reset`, etc.).

```mermaid
sequenceDiagram
    participant Pipe as Pipeline
    participant ModAd as Module Adapter (module_adapter_new)
    participant Mod as Processing Module
    participant Ext as External DSP Module (e.g., Windows APO)

    Pipe->>ModAd: comp_new()
    activate ModAd
    ModAd->>ModAd: Allocates comp_dev & struct processing_module
    ModAd->>ModAd: module_adapter_dp_heap_new() (If DP domain)
    ModAd->>ModAd: pipeline_comp_dp_task_init() (Spins up Zephyr thread if DP)
    ModAd->>Mod: module_init()
    Mod->>Ext: ops->init(mod)
    Ext-->>ModAd: Init OK
    ModAd-->>Pipe: Component READY
    deactivate ModAd

    Note over Pipe,Ext: ... Audio Streaming ...

    Pipe->>ModAd: comp_free()
    activate ModAd
    ModAd->>Mod: module_free()
    Mod->>Ext: ops->free(mod)
    ModAd->>ModAd: schedule_task_free() (terminates thread)
    ModAd->>ModAd: mod_free_all() (cleans up objpool and heap)
    ModAd-->>Pipe: Component Destroyed
    deactivate ModAd
```

## Configuration and Binding Flow

Connection establishes buffer relationships (`comp_buffer`) connecting the component sources and sinks to the rest of the generic pipeline graph.

```mermaid
sequenceDiagram
    participant Pipe as Pipeline
    participant ModAd as Module Adapter
    participant Mod as Processing Module

    Pipe->>ModAd: comp_bind() / comp_unbind()
    activate ModAd
    ModAd->>Mod: module_bind() / module_unbind()
    ModAd->>ModAd: module_update_source_buffer_params()
    ModAd-->>Pipe: Bind successfully mapped
    deactivate ModAd

    Pipe->>ModAd: comp_prepare()
    activate ModAd
    ModAd->>ModAd: Calculate chunks (period bytes, deep_buff_bytes)
    ModAd->>Mod: module_prepare()
    Mod-->>ModAd: External DSP prepared internal states
    ModAd-->>Pipe: Prepared (Buffer formats synced)
    deactivate ModAd
```

## Processing Flow (DP and LL Execution)

To unify components operating directly in interrupt boundaries (DMA Low Latency (LL)) with heavy computational blocks executing asynchronously in Zephyr RTOS threads (Data Processing (DP)), the wrapper intercepts `comp_copy()`.

### Low Latency (LL) Execution

For LL modules, processing is fully synchronous within the pipeline tick:

1. `comp_copy()` invokes `module_adapter_copy()`.
2. Data is fetched directly into `module_process_legacy()` or `module_process_sink_src()`.
3. `ops->process()` consumes upstream arrays and produces downstream arrays.

### Data Processing (DP) Execution

For DP modules, a discrete Zephyr thread manages execution asynchronously:

```mermaid
sequenceDiagram
    participant Sched as LL Scheduler
    participant ModAd as Module Adapter (comp_copy)
    participant DP as DP Thread (dp_task_run)
    participant Mod as Processing Module (Ops)

    Sched->>ModAd: comp_copy()
    activate ModAd
    ModAd->>ModAd: Checks source data & sink free space
    ModAd->>DP: Wakeup background thread (module_adapter_process_dp)
    ModAd-->>Sched: Returns (No computation done yet)
    deactivate ModAd

    activate DP
    DP->>Mod: module_process_sink_src()
    Mod->>Mod: ops->process(sources, sinks)
    Mod-->>DP: Computation Complete
    DP->>Sched: Signals data is ready
    deactivate DP
```

## Error Handling and Memory Sandboxing

* **Sandboxing (`mod_balloc_align`, `z_impl_mod_fast_get`, `z_impl_mod_free`)**: Since third-party DSP code is treated as semi-untrusted in memory lifetimes, module allocations grab slices from a dedicated component `dp_heap_user` heap instead of the global system heap (`mod_heap_info`). The wrapper automatically prunes leaked objects (`mod_free_all(mod)`) during teardown by keeping an `objpool` of all resource containers.
* **Reset Propagation**: Re-initializations via `COMP_TRIGGER_STOP` map down to `module_reset()` clearing the runtime state of the nested DSP module back to `MODULE_INITIALIZED`.

## Configuration and Scripts

* **Kconfig**: Highly customizable environment for "Processing modules" (`COMP_MODULE_ADAPTER`). Provides options for memory allocations, CADENCE codecs (AAC, BSAC, DAB, DRM, MP3, SBC, VORBIS, and their associated libraries), Dolby DAX Audio processing hooks (with stub support), Waves MaxxEffect codec support, and Intel module loaders.
* **CMakeLists.txt**: Handles the sprawling linkage process for the enabled processing modules. Links the core IPC abstraction layers (`module_adapter_ipc3.c` vs `module_adapter_ipc4.c`), external static libraries directly (e.g., `libdax.a`, `libMaxxChrome.a`, arbitrary CADENCE libraries), and includes custom Zephyr build options for `IADK` and `LIBRARY_MANAGER` systems.
