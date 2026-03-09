# DSP Initialization (`src/init`)

The `src/init` directory contains the generic digital signal processor (DSP) initialization code and firmware metadata structures for Sound Open Firmware (SOF). It acts as the bridge between the underlying RTOS (Zephyr) boot phase and the SOF-specific task scheduling and processing pipelines.

## Architecture and Boot Flow

The firmware initialization architecture relies on the Zephyr RTOS boot sequence. Zephyr handles the very early hardware setup and kernel initialization before invoking SOF-specific routines via Zephyr's `SYS_INIT` macros.

### Primary Core Boot Sequence

The primary entry point for SOF system initialization is `sof_init()`, registered to run at Zephyr's `POST_KERNEL` initialization level. This ensures basic OS primitives (like memory allocators and threads) are available before SOF starts.

`sof_init()` delegates to `primary_core_init()`, which executes the following sequence:

```mermaid
sequenceDiagram
    participant Zephyr as Zephyr RTOS
    participant sof_init as sof_init (src/init.c)
    participant primary_core as primary_core_init
    participant trace as trace_init
    participant notify as init_system_notify
    participant pm as pm_runtime_init
    participant platform as platform_init
    participant ams as ams_init
    participant mbox as mailbox (IPC4)
    participant task as task_main_start

    Zephyr->>sof_init: SYS_INIT (POST_KERNEL)
    sof_init->>primary_core: Initialize Primary Core Context

    primary_core->>trace: Setup DMA Tracing & Logging
    primary_core->>notify: Setup System Notifiers
    primary_core->>pm: Initialize Runtime Power Management

    primary_core->>platform: Platform-Specific HW Config
    note right of platform: e.g., interrupts, IPC windows (Intel, i.MX)

    primary_core->>ams: Init Asynchronous Messaging Service

    primary_core->>mbox: Set Firmware Registers ABI Version

    primary_core->>task: Start SOF Main Processing Task Loop
```

1. **Context Initialization**: Sets up the global `struct sof` context.
2. **Logging and Tracing**: Initializes Zephyr's logging timestamps and SOF's DMA trace infrastructure (`trace_init()`), printing the firmware version and ABI banner.
3. **System Subsystems**:
   - Initializes the system notifier (`init_system_notify()`) for inter-component messaging.
   - Sets up runtime power management (`pm_runtime_init()`).
4. **Platform-Specific Initialization**: Calls `platform_init()` to allow the specific hardware platform (e.g., Intel cAVS, i.MX) to configure its hardware IPs, interrupts, and IPC mailbox memory windows.
5. **Architectural Handoff**: For IPC4, it sets the Firmware Registers ABI version in the mailbox. It may also unpack boot manifests if configured.
6. **Task Scheduler**: Finally, it calls `task_main_start()` to kick off the main SOF processing task loop.

### Secondary Core Boot Sequence

For multi-core DSP platforms, secondary cores execute `secondary_core_init()`:

```mermaid
sequenceDiagram
    participant ZephyrN as Zephyr RTOS (Core N)
    participant sci as secondary_core_init
    participant check as check_restore
    participant notify as init_system_notify
    participant ll as scheduler_init_ll
    participant dp as scheduler_dp_init
    participant idc as idc_init

    ZephyrN->>sci: Core Wake/Boot
    sci->>check: Cold boot or power restore?

    alt is Power Restore (e.g. D0ix wake)
        sci->>sci: secondary_core_restore()<br>(Skip full init)
    else is Cold Boot
        sci->>notify: Setup Local Core Notifiers

        sci->>ll: Init Low-Latency (LL) Timer Domain
        sci->>ll: Init LL DMA Domain (if applicable)
        sci->>dp: Init Data Processing (DP) Scheduler

        sci->>idc: Init Inter-Domain Communication (IDC)
    end
```

1. **Power State Checking**: It checks if the core is cold booting or resuming from a low-power retention/restore state (e.g., D0ix) via `check_restore()`. In the restore case, it restores state without re-allocating core structures; in the cold-boot case, it follows the full initialization path described below.
2. **Local Subsystem Setup**: Sets up system notifiers for the local core.
3. **Scheduler Setup**: Initializes the Low-Latency (LL) and Data Processing (DP) schedulers on the secondary core.
4. **Inter-Core Communication**: Initializes the Inter-Domain Communication (IDC) mechanism (`idc_init()`), allowing cross-core messaging.

### Firmware Extended Manifest (`ext_manifest.c`)

This directory also provides the extended manifest implementation. The manifest consists of structured metadata embedded into the `.fw_metadata` section of the firmware binary.

```mermaid
classDiagram
    direction LR

    class fw_metadata {
        <<Section>>
        +ext_man_fw_ver
        +ext_man_cc_ver
        +ext_man_probe
        +ext_man_dbg_info
        +ext_man_config
    }

    class ext_man_elem_header {
        +uint32_t type
        +uint32_t elem_size
    }

    class ext_man_fw_version {
        +ext_man_elem_header hdr
        +sof_ipc_fw_version version
        +uint32_t flags
    }

    class ext_man_cc_version {
        +ext_man_elem_header hdr
        +sof_ipc_cc_version cc_version
    }

    class ext_man_probe_support {
        +ext_man_elem_header hdr
        +sof_ipc_probe_support probe
    }

    class ext_man_dbg_abi {
        +ext_man_elem_header hdr
        +sof_ipc_user_abi_version dbg_abi
    }

    class ext_man_config_data {
        +ext_man_elem_header hdr
        +config_elem elems[]
    }

    fw_metadata *-- ext_man_fw_version
    fw_metadata *-- ext_man_cc_version
    fw_metadata *-- ext_man_probe_support
    fw_metadata *-- ext_man_dbg_abi
    fw_metadata *-- ext_man_config_data

    ext_man_fw_version *-- ext_man_elem_header
    ext_man_cc_version *-- ext_man_elem_header
    ext_man_probe_support *-- ext_man_elem_header
    ext_man_dbg_abi *-- ext_man_elem_header
    ext_man_config_data *-- ext_man_elem_header
```

When the host OS (e.g., Linux SOF driver) parses the firmware binary before loading it, it reads these manifest structures to discover firmware capabilities dynamically. The manifest includes:

- **Firmware Version**: Major, minor, micro, tag, and build hashes (`ext_man_fw_ver`).
- **Compiler Version**: Details of the toolchain used to compile the firmware (`ext_man_cc_ver`).
- **Probe Info**: Extraction probe configurations and limits (`ext_man_probe`).
- **Debug ABI**: Supported debugger ABI versions (`ext_man_dbg_info`).
- **Configuration Dictionary**: Compile-time enabled features and sizing parameters (e.g., `SOF_IPC_MSG_MAX_SIZE`).
