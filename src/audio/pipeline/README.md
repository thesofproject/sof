# Pipeline Engine Architecture

This directory contains the core graph/pipeline logic.

## Overview

The Pipeline engine is the heart of the SOF processing architecture. It links `components` together using `buffers`, and provides scheduling execution entities (tasks) to repeatedly trigger these component graphs.

## Architecture Diagram

```mermaid
graph TD
  Host[Host Topology] -.-> Manager[Pipeline Manager]
  Manager --> Buf1[Buffer]
  Manager --> CompA[Component]
  Manager --> Sched[Task Scheduler]
  Sched --> CompA
```

## State Machine

The pipeline progresses through various states (`COMP_STATE_INIT`, `COMP_STATE_READY`, `COMP_STATE_ACTIVE`, etc.) largely directed by `comp_trigger()` commands cascaded down the graph.

```mermaid
stateDiagram-v2
    [*] --> INIT : pipeline_new()
    INIT --> READY : pipeline_complete()
    READY --> PRE_ACTIVE : COMP_TRIGGER_PRE_START
    PRE_ACTIVE --> ACTIVE : COMP_TRIGGER_START
    ACTIVE --> PAUSED : COMP_TRIGGER_PAUSE
    PAUSED --> ACTIVE : COMP_TRIGGER_RELEASE / START
    ACTIVE --> SUSPEND : COMP_TRIGGER_SUSPEND
    SUSPEND --> ACTIVE : COMP_TRIGGER_RESUME
    ACTIVE --> PRE_ACTIVE : COMP_TRIGGER_PRE_RELEASE
    ACTIVE --> READY : COMP_TRIGGER_STOP
    ACTIVE --> XRUN_PAUSED : COMP_TRIGGER_XRUN
    XRUN_PAUSED --> READY : pipeline_xrun_recover()
```

## Processing Flow (LL and DP Modes)

Execution within the SOF pipeline is divided between two primary timing domains depending on the component's `proc_domain` property.

1. **Low Latency (LL) Domain:**
   * **Driven By:** DMA interrupts or precise timers.
   * **Execution:** A single cooperative scheduler task (`pipeline_task`) iterates over the entire connected graph.
   * **Process:** The pipeline scheduler invokes `pipeline_copy()` which calls `comp_copy()` on the source or sink, and then recursively relies on `pipeline_for_each_comp` to pull or push data through the graph synchronously within that single timeslice.

2. **Data Processing (DP) Domain:**
   * **Driven By:** A Zephyr-based discrete RTOS thread (`CONFIG_ZEPHYR_DP_SCHEDULER`).
   * **Execution:** Modules that require extensive computation (especially those leveraging the `module_adapter`) are spun off into their own isolated threads using `pipeline_comp_dp_task_init()`.
   * **Process:** Instead of synchronous execution alongside the DMA, they consume and produce data (`module_process_sink_src`) with their own stack (`TASK_DP_STACK_SIZE`), relying on inter-component buffers and thread synchronization to pass chunks back to the LL domain when ready.

```mermaid
sequenceDiagram
    participant DMA
    participant Sched as LL Scheduler (pipeline_task)
    participant Source as Source Component (e.g., Host Interface)
    participant Buffer as Internal Buffer
    participant Filter as Filter Component (e.g., EQ)
    participant Sink as Sink Component (e.g., DAI)

    DMA->>Sched: Interrupt / Timer Tick
    activate Sched
    Sched->>Source: pipeline_copy() -> comp_copy()
    Source-->>Buffer: Copies PCM Data to Buffer
    Sched->>Filter: pipeline_for_each_comp() -> comp_copy()
    Filter->>Buffer: Reads PCM Data
    Filter-->>Buffer: Writes Processed Data
    Sched->>Sink: pipeline_for_each_comp() -> comp_copy()
    Sink->>Buffer: Reads Processed Data
    Sink-->>Sink: Transmits to Audio Hardware
    deactivate Sched
```

### Mixed LL and DP Execution

```mermaid
sequenceDiagram
    participant DMA
    participant Sched as LL Scheduler (pipeline_task)
    participant DPThread as DP Thread (dp_task_run)
    participant CompLL as LL Component
    participant CompDP as DP Component

    DMA->>Sched: Interrupt / Timer Tick
    activate Sched
    Sched->>CompLL: pipeline_copy() -> comp_copy()
    CompLL-->>Sched: Data Produced into Buffer
    Sched->>DPThread: Wakeup (Buffer Data Available)
    deactivate Sched

    activate DPThread
    DPThread->>CompDP: module_process_sink_src()
    CompDP-->>DPThread: Computations finished
    DPThread->>Sched: Notification (Data Ready)
    deactivate DPThread
```

## Control and Configuration Flows

The lifecycle of a pipeline graph involves several discrete initialization and connection steps orchestrated by IPC topology commands before streaming begins.

1. **Creation (`pipeline_new`)**: Instantiates the `struct pipeline` object, associates the memory heap, and grabs a mailbox offset for IPC position tracking.
2. **Connection (`pipeline_connect` / `pipeline_disconnect`)**: Establishes the directional edges of the graph by attaching a `comp_buffer` between a source and sink `comp_dev`. It updates the component's internal buffer lists.
3. **Completion (`pipeline_complete`)**: Validates the graph structure. It recursively walks the entire chain from source to sink (`pipeline_for_each_comp`), verifying consistency (e.g., ensuring components aren't part of mismatched pipelines unless properly handled), and transitions the pipeline state to `COMP_STATE_READY`.
4. **Parameter Propagation (`pipeline_params`, `pipeline_prepare`)**: Triggered prior to streaming, `pipeline_prepare()` walks the graph to finalize PCM formats, period sizes, and hardware configurations (like iterating over the audio buffers to `audio_buffer_reset_params`).
5. **Teardown (`pipeline_free`)**: When a stream is closed, after all `comp_dev` objects internal to the pipeline are halted and detached, `pipeline_free` cleans up the `pipe_task` scheduler footprint, IPC messages, and unlinks memory allocations freeing the `struct pipeline` entirely.

### Creation and Teardown Flow

```mermaid
sequenceDiagram
    participant Host
    participant Sched as LL Scheduler
    participant Pipe as Pipeline
    participant Source as Source Component

    Host->>Pipe: pipeline_new()
    activate Pipe
    Pipe-->>Pipe: Allocates struct pipeline & gets Mailbox Offset

    Host->>Source: comp_new() (Creates Components)

    Host->>Pipe: pipeline_connect()
    Pipe-->>Pipe: Links comp_buffers internally

    Host->>Pipe: pipeline_complete()
    Pipe->>Source: pipeline_for_each_comp()
    Source-->>Pipe: Graph validates
    Pipe-->>Host: Status: COMP_STATE_READY

    Note over Host,Pipe: ... Active Audio Streaming ...

    Host->>Pipe: pipeline_free()
    Pipe->>Sched: schedule_task_free(pipe_task)
    Pipe-->>Pipe: sof_heap_free() (Frees tracking structs)
    Pipe-->>Host: Pipeline Extinguished
    deactivate Pipe
```

### Triggering Flow

Triggering is fundamentally responsible for transitioning graph states (`COMP_STATE_ACTIVE`, `COMP_STATE_PAUSED`, etc). A trigger (like `COMP_TRIGGER_START` or `COMP_TRIGGER_STOP`) commands an underlying state change and pushes the `pipeline_task` into the `schedule_task` queue.

```mermaid
sequenceDiagram
    participant Host
    participant PPL as Pipeline (pipeline_trigger)
    participant Sched as LL Scheduler (pipeline_task)
    participant Source as Source Component
    participant Sink as Sink Component

    Host->>PPL: pipeline_trigger(COMP_TRIGGER_START)
    activate PPL
    PPL->>Source: pipeline_for_each_comp(COMP_TRIGGER_START)
    Source->>Sink: comp_trigger(COMP_TRIGGER_START)
    Sink-->>PPL: State -> COMP_STATE_ACTIVE

    PPL->>Sched: pipeline_schedule_copy()
    Sched-->>Sched: Adds `pipe_task` to active scheduler
    PPL-->>Host: Trigger successful
    deactivate PPL

    Note over PPL,Sched: Pipeline repeatedly scheduled via DMA or Timers

    Host->>PPL: pipeline_trigger(COMP_TRIGGER_STOP)
    activate PPL
    PPL->>Sched: pipeline_schedule_cancel()
    Sched-->>Sched: Removes `pipe_task` from active scheduler
    PPL->>Source: pipeline_for_each_comp(COMP_TRIGGER_STOP)
    Source->>Sink: comp_trigger(COMP_TRIGGER_STOP)
    Sink-->>PPL: State -> COMP_STATE_PAUSED / READY
    PPL-->>Host: Trigger successful
    deactivate PPL
```

```mermaid
sequenceDiagram
    participant Host
    participant Pipe as Pipeline (pipeline_complete)
    participant Source as Source Component
    participant Sink as Sink Component
    participant Buf as Buffer

    Host->>Pipe: ipc_pipeline_complete()
    activate Pipe
    Pipe->>Source: pipeline_for_each_comp(PPL_DIR_DOWNSTREAM)
    Source->>Buf: buffer_set_comp()
    Buf->>Sink: comp_is_single_pipeline()
    Sink-->>Pipe: Pipeline graph linked
    Pipe-->>Host: Status: COMP_STATE_READY
    deactivate Pipe

    Host->>Pipe: ipc_comp_prepare()
    activate Pipe
    Pipe->>Source: pipeline_prepare()
    Source->>Buf: audio_buffer_reset_params()
    Buf->>Sink: comp_prepare()
    Sink-->>Pipe: Prepared formats
    Pipe-->>Host: Prepared
    deactivate Pipe
```

## Error Handling (XRUNs)

Overruns (host writes too fast/firmware reads too slow) and underruns (host reads too fast/firmware writes too slow) are tracked continuously.

1. **Detection**: Components directly hooked to interfaces (like a host IPC component or a hardware DAI) monitor their `comp_copy` status. If they detect starvation or overflow, they trigger an XRUN event.
2. **Propagation (`pipeline_xrun`)**: The pipeline immediately invokes a broadcast `pipeline_trigger(..., COMP_TRIGGER_XRUN)` forcing all internal components to drop to a halted `XRUN_PAUSED` state. In older IPC3 topologies, it additionally signals the host via mailbox offsets (`ipc_build_stream_posn`).
3. **Recovery (`pipeline_xrun_handle_trigger` / `pipeline_xrun_recover`)**: By default, the `pipeline_task` scheduler will intercept the `xrun_bytes` flag. Unless `NO_XRUN_RECOVERY` is defined, the firmware attempts self-healing:
   * It resets the pipeline downstream of the source (`pipeline_reset`).
   * It prepares it again (`pipeline_prepare`).
   * It issues an internal `COMP_TRIGGER_START` to restart data flow automatically without host intervention.

```mermaid
sequenceDiagram
    participant DAI as Hardware Interface
    participant Comp as Connected Component
    participant PPL as Pipeline (pipeline_xrun)
    participant Sched as LL Scheduler (pipeline_task)
    participant Host as Host Interface

    DAI-->>Comp: Overflow/Underrun Detected
    activate Comp
    Comp->>PPL: pipeline_xrun()
    PPL->>Comp: pipeline_trigger(COMP_TRIGGER_XRUN)
    PPL-->>Host: Mailbox XRUN position (IPC3)
    deactivate Comp

    Note over PPL,Sched: Pipeline halts (XRUN_PAUSED)

    Sched->>PPL: intercepts xrun_bytes > 0
    activate Sched
    Sched->>PPL: pipeline_xrun_recover()
    PPL->>Comp: pipeline_reset()
    PPL->>Comp: pipeline_prepare()
    PPL->>Comp: pipeline_trigger(COMP_TRIGGER_START)
    PPL-->>Sched: Recovered (xrun_bytes = 0)
    deactivate Sched
```

## Configuration and Scripts

* **CMakeLists.txt**: Straightforward build configuration integrating the fundamental internal execution blocks of the SOF graph: `pipeline-graph.c`, `pipeline-stream.c`, `pipeline-params.c`, `pipeline-xrun.c`, and `pipeline-schedule.c`.
