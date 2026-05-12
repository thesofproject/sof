# SOF Zephyr Shell Commands

This document describes all SOF-specific shell commands available in the Zephyr RTOS environment. These commands are grouped under the `sof` parent command and provide diagnostic visibility into the firmware's runtime state.

## Usage

Access the Zephyr shell through the QEMU terminal or a hardware UART console. Prefix all commands with `sof`.

```shell
uart:~$ sof <command> [arguments]
```

## Available SOF Commands

### 1. `sof version`
- **Description**: Prints the firmware version details.
- **Usage**: `sof version`
- **Output**: Major/Minor/Micro version, Git tag, and source hash.

### 2. `sof mod heap usage`
- **Description**: Dumps heap memory usage for all active audio modules.
- **Usage**: `sof mod heap usage`
- **Output**: Component ID, current heap usage, and high-water mark (HWM) in bytes.

### 3. `sof pipeline_list`
- **Description**: Lists all active audio pipelines in the system.
- **Usage**: `sof pipeline_list`
- **Output**: Pipeline ID, Core affinity, Status, Priority, and Period (us).

### 4. `sof module_list`
- **Description**: Lists all instantiated audio modules (components).
- **Usage**: `sof module_list`
- **Output**: Component ID, Module Type, State, Pipeline ID, and Core affinity.

### 5. `sof vpage_status`
- **Description**: Reports the status of the virtual page allocator.
- **Usage**: `sof vpage_status`
- **Output**: Base address, total/free pages, and a list of active virtual page allocations.
- **Dependency**: Requires `CONFIG_SOF_VREGIONS=y`.

### 6. `sof vregion_status`
- **Description**: Reports status and metrics for all active virtual memory regions.
- **Usage**: `sof vregion_status`
- **Output**: Region base addresses, sizes, lifetime bytes used/free, and current reference counts.
- **Dependency**: Requires `CONFIG_SOF_VREGIONS=y`.

### 7. `sof test_inject_sched_gap`
- **Description**: Injects a timing delay into the audio scheduling domain for stress testing.
- **Usage**: `sof test_inject_sched_gap [usec]`
- **Arguments**: `usec` (optional, default 1500) - microseconds to block the domain.
- **Warning**: Not reliable on SMP systems without explicit cross-core support.

### 8. `sof pipeline_status`
- **Description**: Lists all active IPC4 pipelines with state, priority, period and core.
- **Usage**: `sof pipeline_status`
- **Output**: `ppl_id  core  priority  period_us  status  state`
- **Dependency**: Requires `CONFIG_SOF_SHELL_PIPELINE_STATUS=y`.

### 9. `sof module_status`
- **Description**: Lists all instantiated IPC4 components with pipeline, core and state.
- **Usage**: `sof module_status`
- **Output**: `comp_id  module  ppl_id  core  state`
- **Dependency**: Requires `CONFIG_SOF_SHELL_MODULE_STATUS=y`.

### 10. `sof core_status`
- **Description**: Reports the enabled/active state of each DSP core.
- **Usage**: `sof core_status`
- **Output**: Per-core enabled flag and which core is currently executing.
- **Dependency**: Requires `CONFIG_SOF_SHELL_CORE_STATUS=y`.

### 11. `sof core_on`
- **Description**: Powers on a secondary DSP core.
- **Usage**: `sof core_on <core_id>`
- **Arguments**: `core_id` — 1 to `CONFIG_CORE_COUNT-1` (core 0 is always on).
- **Dependency**: Requires `CONFIG_SOF_SHELL_CORE_POWER=y`.

### 12. `sof core_off`
- **Description**: Powers off a secondary DSP core.
- **Usage**: `sof core_off <core_id>`
- **Arguments**: `core_id` — 1 to `CONFIG_CORE_COUNT-1`.
- **Dependency**: Requires `CONFIG_SOF_SHELL_CORE_POWER=y`.

### 13. `sof sram_status`
- **Description**: Reports HPSRAM heap allocated, free, and peak-allocated bytes.
- **Usage**: `sof sram_status`
- **Dependency**: Requires `CONFIG_SOF_SHELL_SRAM_STATUS=y` and `CONFIG_SYS_HEAP_RUNTIME_STATS=y`.

### 14. `sof clock_status`
- **Description**: Prints the current CPU clock frequency for each DSP core.
- **Usage**: `sof clock_status`
- **Output**: `clock  freq_hz  freq_mhz` per core.
- **Dependency**: Requires `CONFIG_SOF_SHELL_CLOCK_STATUS=y` and `!CONFIG_SOF_ZEPHYR_NO_SOF_CLOCK`.

### 15. `sof mmu_status`
- **Description**: Prints Intel ADSP MTL TLB / virtual memory status including VM base, page count, mapped/free pages, TLB MMIO base, and the `sys_mm_drv` mapped memory region list.
- **Usage**: `sof mmu_status`
- **Dependency**: Requires `CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB=y`.

### 16. `sof tlb_dump`
- **Description**: Dumps all active TLB entries showing virtual address, physical address, RWX flags and raw 16-bit entry value.
- **Usage**: `sof tlb_dump`
- **Dependency**: Requires `CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB=y`.

### 17. `sof tlb_lookup`
- **Description**: Queries the TLB for a single page or a VA range.
- **Usage**: `sof tlb_lookup <vaddr> [end_vaddr]`
- **Output**: Per-page: virtual address, physical address, mapped flag, RWX flags, bank index, raw entry.
- **Dependency**: Requires `CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB=y`.

### 18. `sof rasid`
- **Description**: Decodes the Xtensa RASID hardware register showing the ring→ASID mapping.
- **Usage**: `sof rasid`
- **Output**: Raw RASID value plus per-ring (kernel/unused/user/shared) ASID byte.
- **Dependency**: Requires `CONFIG_XTENSA_MMU=y`.

### 19. `sof page info`
- **Description**: Probes the Xtensa DTLB for a page or address range and reports physical address, ring, ASID, cache mode (WB/WT/uncached/illegal) and RWX permissions.
- **Usage**: `sof page info <vaddr> [end_vaddr]`
- **Dependency**: Requires `CONFIG_XTENSA_MMU=y`.

### 20. `sof llext_load`
- **Description**: Initiates an interactive llext module load from the host via the ADSP debug-window DMA slot. Sets up the handshake then waits (up to 120 s) for the host to DMA the `.ri` file.
- **Usage**: `sof llext_load <name> [lib_id=1]`
- **Host side**: `dd if=<module.ri> of=/sys/kernel/debug/sof/llext_load bs=$(stat -c%s <module.ri>) count=1`
- **Dependency**: Requires `CONFIG_SOF_SHELL_LLEXT_LOAD=y` and `CONFIG_LIBRARY_MANAGER=y`.

### 21. `sof llext_list`
- **Description**: Lists all llext libraries currently held in IMR/DRAM. For each library shows base address, storage size, number of module files, and per-file DRAM/SRAM mapping state, use count and dependency count.
- **Usage**: `sof llext_list`
- **Dependency**: Requires `CONFIG_SOF_SHELL_LLEXT_LIST=y` and `CONFIG_LIBRARY_MANAGER=y`.

### 22. `sof llext_purge`
- **Description**: Removes a loadable llext library from IMR/DRAM and frees its memory. Fails with `-EBUSY` if any module file is still mapped in SRAM.
- **Usage**: `sof llext_purge <lib_id>`
- **Dependency**: Requires `CONFIG_SOF_SHELL_LLEXT_PURGE=y` and `CONFIG_LIBRARY_MANAGER=y`.

### 23. `sof ppl_create`
- **Description**: Creates a new IPC4 pipeline instance. Intended for bring-up and debug use only.
- **Usage**: `sof ppl_create <ppl_id> [priority=0] [pages=2] [core=0] [lp=0]`
- **Arguments**: `ppl_id` (0–255), `priority` (0–7), `pages` (1–2047), `core` (0–7), `lp` (0/1 — low-power mode).
- **Dependency**: Requires `CONFIG_SOF_SHELL_PIPELINE_OPS=y`.

### 24. `sof ppl_delete`
- **Description**: Deletes a pipeline and all its module instances.
- **Usage**: `sof ppl_delete <ppl_id>`
- **Dependency**: Requires `CONFIG_SOF_SHELL_PIPELINE_OPS=y`.

### 25. `sof ppl_state`
- **Description**: Transitions a pipeline to a new IPC4 state.
- **Usage**: `sof ppl_state <ppl_id> <running|paused|reset>`
- **Dependency**: Requires `CONFIG_SOF_SHELL_PIPELINE_OPS=y`.

### 26. `sof mod_init`
- **Description**: Instantiates a module into a pipeline. Module ID can be given as a decimal/hex number or as the 8-char manifest name. No init-data blob is sent; only modules with a working zero-parameter default config will succeed.
- **Usage**: `sof mod_init <mod_name|mod_id> <inst_id> <ppl_id> [core=0] [dp=0]`
- **Output**: Prints `module NAME(0xID) inst N created in pipeline P comp_id=0x...` on success.
- **Dependency**: Requires `CONFIG_SOF_SHELL_PIPELINE_OPS=y`.

### 27. `sof mod_delete`
- **Description**: Deletes a module instance.
- **Usage**: `sof mod_delete <mod_name|mod_id> <inst_id>`
- **Dependency**: Requires `CONFIG_SOF_SHELL_PIPELINE_OPS=y`.

### 28. `sof mod_bind`
- **Description**: Binds (connects) the output queue of a source module to the input queue of a destination module.
- **Usage**: `sof mod_bind <src_name|src_id> <src_inst> <dst_name|dst_id> <dst_inst> [src_queue=0] [dst_queue=0]`
- **Output**: `bound NAME(0xID):INST[qN] -> NAME(0xID):INST[qN]`
- **Dependency**: Requires `CONFIG_SOF_SHELL_PIPELINE_OPS=y`.

### 29. `sof mod_unbind`
- **Description**: Disconnects two previously bound module instances.
- **Usage**: `sof mod_unbind <src_name|src_id> <src_inst> <dst_name|dst_id> <dst_inst> [src_queue=0] [dst_queue=0]`
- **Output**: `unbound NAME(0xID):INST[qN] -/- NAME(0xID):INST[qN]`
- **Dependency**: Requires `CONFIG_SOF_SHELL_PIPELINE_OPS=y`.

---

## Enabling Shell Commands

Ensure the following Kconfig symbols are enabled in your build configuration:
- `CONFIG_SHELL=y`
- `CONFIG_SOF_VREGIONS=y` (for `vpage_status` and `vregion_status`)
- `CONFIG_SOF_SHELL_PIPELINE_STATUS=y` / `CONFIG_SOF_SHELL_MODULE_STATUS=y`
- `CONFIG_SOF_SHELL_CORE_STATUS=y` / `CONFIG_SOF_SHELL_CORE_POWER=y`
- `CONFIG_SOF_SHELL_SRAM_STATUS=y` + `CONFIG_SYS_HEAP_RUNTIME_STATS=y`
- `CONFIG_SOF_SHELL_CLOCK_STATUS=y` (skipped when `CONFIG_SOF_ZEPHYR_NO_SOF_CLOCK=y`)
- `CONFIG_SOF_SHELL_MMU_DBG=y` (for `mmu_status`, `tlb_dump`, `tlb_lookup` on MTL/ARL; `rasid`, `page info` on PTL Xtensa MMU platforms)
- `CONFIG_SOF_SHELL_LLEXT_LOAD=y` / `CONFIG_SOF_SHELL_LLEXT_LIST=y` / `CONFIG_SOF_SHELL_LLEXT_PURGE=y` (need `CONFIG_LIBRARY_MANAGER=y`)
- `CONFIG_SOF_SHELL_PIPELINE_OPS=y` (for `ppl_create/delete/state`, `mod_init/delete/bind/unbind`; default `n`, depends `IPC_MAJOR_4`)

---

## Functional Areas Missing Shell Coverage

Tracking list of subsystems that lack runtime shell visibility for control,
debug or testing. Items get ticked off as commands land on `topic/shell`.

### Control

| Area | Suggested commands | Status |
|---|---|---|
| **kcontrols / mixer** | `kctl list`, `kctl_get <id>`, `kctl_set <id> <val>` | **DONE (task 8)** &mdash; `kctl list` decodes module names + kind for every component. `kctl_get/set` deferred (per-module IPC4 large_config blobs &mdash; use host tools). |
| Module runtime config | `mod_config_get/set <mod_id> <inst> <param_id>` | TODO |
| Stream / copier | `stream_list`, `stream_pause/resume <id>`, `copier_gain_set` | TODO |
| Clocks (extend `clock_status`) | `clock_set <core> <freq>`, `clock_force <pll>` | TODO |
| Power management | `pm_state`, `pm_force <state>`, `pg_status`, `idle_stats` | TODO |
| Cache | `dcache_flush <addr> <size>`, `dcache_inv`, `icache_inv` | TODO |
| Watchdog | `wdt_status`, `wdt_kick`, `wdt_disable` | TODO |
| **DAI / link control** | `dai_list`, `dai_status <type> <idx>`, `dai_trigger`, `dai_loopback` | **DONE (task 7)** &mdash; `dai_list` covers introspection; trigger/loopback deferred (writeable, needs careful tplg coordination). |
| **DMA** | `dma_list`, `dma_chan_status <dma> <chan>`, `dma_stop` | **DONE (task 7)** &mdash; `dma status` covers list+per-channel. `dma_stop` deferred (would corrupt active stream). |

### Debug

| Area | Suggested commands | Status |
|---|---|---|
| **IPC counters / last message** | `ipc stats`, `ipc last` | **DONE (task 1)** |
| IPC inject | `ipc_inject <hex>`, `ipc_queue` | TODO |
| Audio buffers | `buffer list`, `buffer info <id>` | **DONE (task 2)** |
| **Scheduler** | `sched tasks`, `sched load`, `task_info <task>` | **DONE (task 3)** |
| Logging / trace | `log status`, `mtrace dump` (snapshot) | **DONE (task 4)** &mdash; runtime per-source `log_level` deferred (needs `CONFIG_LOG_RUNTIME_FILTERING`, see notes) |
| **Telemetry / perf** | `perf status`, `perf status reset`, `perf status start/stop/pause` | **DONE (task 6)** |
| Notifications | `notify_subscribers`, `notify_stats` | TODO |
| **Debug window / mailbox** | `dbgwin dump <slot>`, `mailbox hex` | **DONE (task 5)** |
| Crash / panic | `crash_log`, `crash_clear`, `panic_info`, `bt`, `regs` | TODO |
| Heap walk | `heap_walk <zone>`, `heap_blocks`, `obj_pool_stats` | TODO |
| Probes | `probe_init`, `probe_add <buf_id>`, `probe_remove`, `probe_dma_status` | TODO |
| Locks / IRQ / IDC | `mutex_stats`, `irq_stats`, `idc_stats` | TODO |

### Testing / fault injection

| Area | Suggested commands | Status |
|---|---|---|
| Fault injection | `test_alloc_fail <count>`, `test_ipc_drop`, `test_dma_stall`, `test_xrun <buf>`, `test_panic` | TODO |
| Self-test | `selftest dma`, `selftest mem`, `selftest cache`, `selftest mmu`, `selftest llext` | TODO |
| Module unit-tests | `test_module <name>` | TODO |
| Loopback / signal gen | `tone_play <freq>`, `loopback_start <src> <sink>`, `pcm_capture_dump` | TODO |
| Mock IPC4 payloads | `ipc_replay <file>` (via DMA slot) | TODO |
| Coverage / hooks | `cov_dump`, `assert_count`, `assert_clear` | TODO |

### Quick-win order

1. **`ipc stats` / `ipc last`** &mdash; DONE.
2. **`buffer list` / `buffer info`** &mdash; DONE.
3. **`sched tasks` / `sched load`** &mdash; DONE.
4. **`log status` / `mtrace dump`** &mdash; DONE.
5. **`mailbox hex` / `dbgwin dump`** &mdash; DONE (was originally `crash_log`/`bt`; pivoted because SOF panic.c isn't built on Zephyr and `bt` of a running CPU from itself isn't meaningful).
6. **`perf status`** &mdash; DONE.
7. **`dai_list` / `dma status`** &mdash; DONE.
8. **`kctl_get/set`** &mdash; DONE (`kctl list` only; values stay on host).

---

## Task 1 &mdash; `ipc stats` / `ipc last`

### Commands

| Command | Description |
|---|---|
| `sof ipc stats` | Print RX/TX counters (`rx_count`, `rx_errors`, `tx_count`, `tx_direct_count`, `tx_errors`). |
| `sof ipc stats reset` | Clear all counters. |
| `sof ipc last` | Print the most recent RX and TX `pri`/`ext` headers and their platform-cycle timestamps. |

### Implementation

- Public API in `src/include/sof/ipc/common.h`:
  - `struct ipc_stats`
  - `ipc_stats_record_rx(pri, ext)`
  - `ipc_stats_record_tx(pri, ext, direct, err)`
  - `ipc_stats_inc_rx_error()`
  - `ipc_stats_get(out)` / `ipc_stats_reset()`
- Storage and locking in `src/ipc/ipc-common.c` (single global, protected by
  a dedicated `g_ipc_stats_lock` spinlock; using ipc->lock was avoided to
  prevent lock recursion in the IPC hot path).
- RX hook: `ipc_platform_do_cmd()` in `src/ipc/ipc-zephyr.c`, just before
  dispatch &mdash; captures the IPC3/IPC4 `pri`/`ext` words.
- TX hooks: `ipc_platform_send_msg()` and `ipc_platform_send_msg_direct()` in
  the same file, after the platform send returns.
- Error hook: `ipc_cmd()` default branch in `src/ipc/ipc4/handler-kernel.c`
  for unknown message targets.
- Shell commands in [zephyr/sof_shell.c](zephyr/sof_shell.c).

### Notes / follow-ups

- Counters are 32-bit; they wrap at ~4 billion messages.
- IPC3 dispatch errors are not yet routed through `ipc_stats_inc_rx_error()`.
- A future task can add a small ring buffer of last-N IPC headers and
  per-target counters (FW_GEN_MSG vs MODULE_MSG, plus per opcode).

## Task 2 &mdash; `buffer list` / `buffer info`

### Commands

| Command | Description |
|---|---|
| `sof buffer list` | List every audio buffer in the pipeline with source/sink component IDs, size, avail, free, channels, rate and frame format. |
| `sof buffer info <buffer_id>` | Detailed info for a single buffer: source/sink comps, core, flags, size/avail/free bytes, rptr, wptr, channels, rate, frame format. |

### Implementation

- Buffers are enumerated by walking `ipc->comp_list`; for each
  `COMP_TYPE_COMPONENT` we walk its `bsink_list` via
  `comp_dev_get_first_data_consumer()` /
  `comp_dev_get_next_data_consumer()`.  Each buffer therefore appears
  exactly once (it is the sink of exactly one source component) and the
  same enumeration works on both IPC3 and IPC4.
- `buf_get_id()` from [src/include/sof/audio/buffer.h](src/include/sof/audio/buffer.h)
  is used as the buffer identifier.
- Stream metrics use the existing `audio_stream_get_*()` accessors from
  [src/include/sof/audio/audio_stream.h](src/include/sof/audio/audio_stream.h).
- New Kconfig `CONFIG_SOF_SHELL_BUFFER_INFO` (default `y`).
- Shell commands in [zephyr/sof_shell.c](zephyr/sof_shell.c).

### Notes / follow-ups

- Today only fill-level snapshots are reported; high-water mark and
  underrun/overrun counters are not tracked in `comp_buffer` and would
  require new instrumentation.
- `buffer info` does not yet decode `flags` symbolically.
- A future enhancement could add `--core <n>` filtering and per-buffer
  topology graph output.

## Task 3 &mdash; `sched tasks` / `sched load`

### Commands

| Command | Description |
|---|---|
| `sof sched tasks` | List every task across all SOF schedulers (LL timer, LL DMA, EDF, DP, TWB) with type, core, priority, state, flags and uid. |
| `sof sched load`  | Per-task cycle counters (cycles_cnt, cycles_sum, cycles_max, derived average) plus aggregate totals. Pairs with `test_inject_sched_gap`. |

### Implementation

- New optional op `scheduler_dump_tasks(data, cb, ctx)` added to
  `struct scheduler_ops` in
  [src/include/sof/schedule/schedule.h](src/include/sof/schedule/schedule.h).
- Implemented for the Zephyr schedulers under their own locks:
  - [src/schedule/zephyr_ll.c](src/schedule/zephyr_ll.c) (LL timer / LL DMA)
  - [src/schedule/zephyr_twb_schedule.c](src/schedule/zephyr_twb_schedule.c)
  - [src/schedule/zephyr_dp_schedule.c](src/schedule/zephyr_dp_schedule.c)
- Shell walks the global scheduler list via `arch_schedulers_get()` and
  invokes the op on every scheduler that provides one; schedulers
  without an implementation are silently skipped.
- Cycle counters are read from existing `task->cycles_sum`,
  `task->cycles_max`, `task->cycles_cnt` fields already maintained by
  the schedulers.
- New Kconfig `CONFIG_SOF_SHELL_SCHED_INFO` (default `y`).
- Shell commands in [zephyr/sof_shell.c](zephyr/sof_shell.c).

### Notes / follow-ups

- The xtos LL scheduler is not yet covered (not built on Zephyr ACE
  targets).
- `task_info <uid>` lookup, deadline-miss counts and per-core
  aggregation could be added on top of the same op.

## Task 4 &mdash; `log status` / `mtrace dump`

### Commands

| Command | Description |
|---|---|
| `sof log status` | List every Zephyr log backend (idx, internal id, active state, name) plus the total number of registered log sources in the local domain. Read-only. |
| `sof mtrace dump` | Print the unread portion of the ADSP mtrace SRAM ring buffer as a snapshot, *without* advancing `host_ptr`. Safe to use while host-side `mtrace-reader.py` is running. |

### Implementation

- `log status` uses Zephyr's public log backend API
  (`log_backend_count_get()`, `log_backend_get()`,
  `log_backend_is_active()`, `log_backend_id_get()`,
  `log_src_cnt_get()`); no new state is added.
- `mtrace dump` re-acquires the existing mtrace slot:
  - With `CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER=y` (default on PTL/MTL/LNL):
    via `adsp_dw_request_slot()` with the same descriptor type
    (`ADSP_DW_SLOT_DEBUG_LOG | core 0`); the slot manager returns the
    already-allocated slot.
  - Otherwise: directly indexes
    `ADSP_DW->slots[ADSP_DW_SLOT_NUM_MTRACE]`.
  - The slot layout (`{host_ptr, dsp_ptr, data[]}`) mirrors the one in
    [zephyr/subsys/logging/backends/log_backend_adsp_mtrace.c](../../zephyr/subsys/logging/backends/log_backend_adsp_mtrace.c).
  - We read from `host_ptr` to `dsp_ptr` byte-by-byte and write to the
    shell, but never store back to `host_ptr`, so the host-side
    consumer keeps seeing the same bytes.
- Two new Kconfigs (default `y`):
  - `CONFIG_SOF_SHELL_LOG_INFO` (depends on `LOG`)
  - `CONFIG_SOF_SHELL_MTRACE_DUMP` (depends on `LOG_BACKEND_ADSP_MTRACE`)
- Shell commands in [zephyr/sof_shell.c](zephyr/sof_shell.c).

### Notes / follow-ups

- Per-source runtime `log_level` setting was deliberately deferred: it
  requires `CONFIG_LOG_RUNTIME_FILTERING=y` (extra per-call overhead)
  and Zephyr already ships an equivalent `log` shell command
  (`CONFIG_LOG_CMDS=y`). If we ever need it we should reuse Zephyr's
  command rather than reimplement it.
- `mtrace dump` shows raw text exactly as the backend formatted it; a
  future option could format output in pages or filter by severity.
- A `mtrace dump --consume` mode (advance `host_ptr`) is intentionally
  not provided to avoid silently breaking host-side tooling.

## Task 5 &mdash; `mailbox hex` / `dbgwin dump`

### Commands

| Command | Description |
|---|---|
| `sof mailbox hex` | List the four SOF mailbox regions (exception, dspbox, hostbox, debug) with their base address and size. |
| `sof mailbox hex <region> [off] [len]` | Hex-dump a mailbox region; offset and length are clamped to the region size. Default length 256 bytes. |
| `sof dbgwin dump` | List all 15 ADSP debug-window slot descriptors (resource_id, type, vma, decoded type name, core). |
| `sof dbgwin dump <slot> [len]` | Hex-dump a single slot (max `ADSP_DW_SLOT_SIZE` = 4096 bytes); default length 256. |

### Implementation

- `mailbox hex` uses the `MAILBOX_*_BASE` / `MAILBOX_*_SIZE` macros
  from [src/include/sof/lib/mailbox.h](src/include/sof/lib/mailbox.h);
  the four region records are a static table.
- `dbgwin dump` re-derives the window 2 base from the device tree
  (`mem_window2`) plus `WIN2_OFFSET`, mirrors
  `struct adsp_debug_window` from
  [zephyr/soc/intel/intel_adsp/common/debug_window.c](../../zephyr/soc/intel/intel_adsp/common/debug_window.c)
  and reads through an uncached pointer so we always see the
  slot-manager state (works whether or not
  `CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER=y`).
- A small shared `sof_shell_hex_dump()` helper handles the 16-byte
  hex+ASCII rows and is built whenever either command is enabled.
- Two new Kconfigs (default `y`):
  - `CONFIG_SOF_SHELL_MAILBOX_HEX`
  - `CONFIG_SOF_SHELL_DBGWIN_DUMP` (depends on `SOC_FAMILY_INTEL_ADSP`)
- Shell commands in [zephyr/sof_shell.c](zephyr/sof_shell.c).

### Notes / follow-ups

- The original quick-win was `crash_log`/`bt`; pivoted because SOF's
  in-tree `panic_dump()` is not compiled on Zephyr (Zephyr installs
  its own fatal handler) and a running shell can't backtrace its own
  CPU after a panic. `mailbox hex exception` still surfaces whatever
  the platform-specific fatal path leaves there, so the same
  diagnostic intent is covered as far as it can be from a live shell.
- A future `panic_decode` could parse a known on-target oops layout
  (Zephyr coredump or telemetry slot) once one is standardised on
  ACE.
- `dbgwin dump` is read-only. We do not implement a write/seize
  command to avoid corrupting host-visible state.

## Task 6 &mdash; `perf status`

### Commands

| Command | Description |
|---|---|
| `sof perf status` | Print the SOF telemetry performance state (`disabled`/`stopped`/`started`/`paused`) and per-active-core systick counters (`count`, `last_time_elapsed`, `max_time_elapsed`, `avg_kcps`, `peak_kcps`, plus 4k/8k peak utilization). |
| `sof perf status reset` | Call `reset_performance_counters()` to zero all counters. |
| `sof perf status start` | Call `enable_performance_counters()` and set state to `STARTED`. |
| `sof perf status stop` / `pause` | Transition state to `STOPPED` or `PAUSED` (stops sampling without zeroing counters). |

### Implementation

- Reads per-core systick info via
  `telemetry_get_systick_info_ptr()` (with
  `CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER`) or directly from
  `ADSP_DW->slots[SOF_DW_TELEMETRY_SLOT]` otherwise.
- Iterates only cores in `cpu_enabled_cores()` so the output matches
  the active topology.
- Uses the existing `perf_meas_get_state()` /
  `perf_meas_set_state()` /
  `enable_performance_counters()` /
  `reset_performance_counters()` API from
  [src/include/sof/debug/telemetry/performance_monitor.h](src/include/sof/debug/telemetry/performance_monitor.h);
  no new state added.
- New Kconfig `CONFIG_SOF_SHELL_PERF_STATUS` (default `y`,
  depends on `SOF_TELEMETRY`).
- Shell command in [zephyr/sof_shell.c](zephyr/sof_shell.c).

### Notes / follow-ups

- We deliberately do not dump the full per-component
  `perf_data_item_comp` array yet: it can grow large
  (`CONFIG_MEMORY_WIN_3_SIZE` / item size, ~hundreds of items on PTL)
  and would require a heap allocation. A future `perf_components` /
  `perf status -v` could iterate `performance_data_bitmap` and stream
  one row per occupied slot.
- Zephyr already provides `kernel cpu_load` and `kernel threads`;
  `cpu_load` was therefore not duplicated here.

## Task 7 &mdash; `dai_list` / `dma status`

### Commands

| Command | Description |
|---|---|
| `sof dai_list` | Iterate `dai_get_device_list()` and print, per DAI, the Zephyr device name, decoded type (ssp/dmic/hda/alh/uaol/sai/esai/...), index, current channel count, sample rate, format and word size, plus per-direction fifo address, fifo depth, DMA handshake id and stream id. |
| `sof dma status` | List every SOF DMA controller (`dma_info_get()`), with id, channel count, busy count, caps/devs bitmasks, base address and Zephyr device name. |
| `sof dma status <dma>` | Walk all channels of one controller, calling `sof_dma_get_status()` on each. |
| `sof dma status <dma> <chan>` | Status of a single channel: busy/idle, direction, pending/free bytes, read/write positions, total_copied. |

### Implementation

- `dai_list` uses `dai_get_device_list()` from
  [src/include/sof/lib/dai-zephyr.h](src/include/sof/lib/dai-zephyr.h)
  and the Zephyr DAI API
  (`dai_config_get()`, `dai_get_properties()`); it falls back to
  TX-only or RX-only `config_get()` when `DAI_DIR_BOTH` is not
  supported by a driver.
- `dma status` walks `sof_get()->dma_info->dma_array[]` (via
  `dma_info_get()` from
  [zephyr/include/sof/lib/dma.h](zephyr/include/sof/lib/dma.h))
  and calls `sof_dma_get_status()` per channel; this re-uses the
  same Zephyr `dma_get_status()` path the DSP itself uses, so the
  numbers exactly match runtime audio state.
- Two new Kconfigs (default `y`, both depend on
  `ZEPHYR_NATIVE_DRIVERS`):
  - `CONFIG_SOF_SHELL_DAI_LIST`
  - `CONFIG_SOF_SHELL_DMA_STATUS`
- Shell commands in [zephyr/sof_shell.c](zephyr/sof_shell.c).

### Notes / follow-ups

- Read-only on purpose. `dai_trigger`, `dai_loopback`, `dma_stop`
  were intentionally not added in this pass &mdash; they would corrupt
  in-flight streams and require coordination with topology / IPC
  state machines. Pair with the existing `pipeline_state` (gated by
  `CONFIG_SOF_SHELL_PIPELINE_OPS`) for stream control.
- `dma status` only iterates SOF-registered DMACs. Zephyr also
  ships its own `dma` shell when `CONFIG_DMA_SHELL=y`, but that one
  walks Zephyr DMA devices and exposes raw register pokes, so the
  two are complementary.

## Task 8 &mdash; `kctl list`

### Commands

| Command | Description |
|---|---|
| `sof kctl list` | Walk every component in the IPC topology and print `comp_id`, `pipeline_id`, `core`, the decoded module name (`volume`, `gain`, `mixin`, `mixout`, `eqiir`, `src`, ...), a coarse `kind` tag for control-bearing modules (`volume` / `mixer` / `blob` / `config`) and the current `comp_state`. |

### Implementation

- Module-adapter components all share `SOF_COMP_MODULE_ADAPTER` for
  `drv->type`, so the only stable per-module label available in
  firmware is the UUID name string from
  `cd->drv->tctx->uuid_p->name` (the same name the LDC tool prints).
  `kctl_drv_name()` reads that, `kctl_drv_kind()` maps known module
  names to a coarse control-family tag.
- New Kconfig (default `y`, depends on `SHELL`):
  `CONFIG_SOF_SHELL_KCTL_LIST`.
- Shell command in [zephyr/sof_shell.c](zephyr/sof_shell.c).

### Notes / follow-ups

- Read-only on purpose. `kctl_get` / `kctl_set` are intentionally
  not implemented in firmware. Control values flow through
  per-module IPC4 large_config blobs
  (`set_configuration` / `get_configuration` in
  [src/include/module/module/interface.h](src/include/module/module/interface.h)),
  each with their own `config_id` namespace and TLV layout.
  Marshalling that from the shell would essentially duplicate the
  host-side tplg / IPC code path. Use `tinymix` /
  [sof-ctl](tools/ctl) on the host instead, and pair with
  `sof module_status` for raw component state.
- This concludes the documented quick-win list. Future shell
  commands should follow the same pattern: small, read-only,
  Kconfig-gated, and complementary to (not a replacement for) the
  host control plane.

---

## DSP Status Commands (`pipeline_status`, `module_status`, `core_status/on/off`, `sram_status`, `clock_status`)

These commands provide a read-only snapshot of the firmware runtime state.

| Command | Kconfig | Description |
|---|---|---|
| `sof pipeline_status` | `SOF_SHELL_PIPELINE_STATUS` | IPC4 pipeline list: id, core, priority, period_us, numeric status, decoded state string. |
| `sof module_status` | `SOF_SHELL_MODULE_STATUS` | IPC4 component list: comp_id, module name + id, ppl_id, core, state. |
| `sof core_status` | `SOF_SHELL_CORE_STATUS` | Per-core enabled flag and current-core marker. |
| `sof core_on <id>` | `SOF_SHELL_CORE_POWER` | Power on secondary core via `cpu_enable_core()`. |
| `sof core_off <id>` | `SOF_SHELL_CORE_POWER` | Power off secondary core via `cpu_disable_core()`. |
| `sof sram_status` | `SOF_SHELL_SRAM_STATUS` | HPSRAM `sys_heap` stats (allocated / free / max_allocated). Needs `SYS_HEAP_RUNTIME_STATS=y`. |
| `sof clock_status` | `SOF_SHELL_CLOCK_STATUS` | Clock frequency per DSP core from `clocks_get()`. Skipped when `SOF_ZEPHYR_NO_SOF_CLOCK=y`. |

Shell commands in [zephyr/sof_shell.c](zephyr/sof_shell.c).

---

## MMU / TLB Debug Commands (`mmu_status`, `tlb_dump`, `tlb_lookup`, `rasid`, `page info`)

Gated by `CONFIG_SOF_SHELL_MMU_DBG`. The available sub-set depends on the
platform MMU driver selected:

| Command | Platform | Kconfig guard |
|---|---|---|
| `sof mmu_status` | Intel ADSP MTL/ARL (TLB MMIO) | `MM_DRV_INTEL_ADSP_MTL_TLB` |
| `sof tlb_dump` | Intel ADSP MTL/ARL | `MM_DRV_INTEL_ADSP_MTL_TLB` |
| `sof tlb_lookup <va> [end_va]` | Intel ADSP MTL/ARL | `MM_DRV_INTEL_ADSP_MTL_TLB` |
| `sof rasid` | PTL / Xtensa MMU | `XTENSA_MMU` |
| `sof page info <va> [end_va]` | PTL / Xtensa MMU | `XTENSA_MMU` |

On PTL both sets of commands are available (platform has both the TLB MMIO and
the Xtensa hardware MMU).

### Implementation notes

- `mmu_status` / `tlb_dump` / `tlb_lookup` read the 16-bit MMIO TLB table
  directly via the `mm-drv-intel-adsp` device-tree node; no `sys_mm_drv` calls
  are needed.
- `rasid` uses the `rsr rasid` Xtensa instruction; `page info` uses `pdtlb` /
  `rdtlb0` / `rdtlb1` inline assembly to probe the hardware DTLB.
- All commands are purely read-only — they do not modify TLB entries or RASID.

---

## llext Commands (`llext_load`, `llext_list`, `llext_purge`)

These commands manage loadable extension (llext) firmware libraries.

| Command | Kconfig | Description |
|---|---|---|
| `sof llext_load <name> [lib_id]` | `SOF_SHELL_LLEXT_LOAD` | Interactive DMA-based load from host. Signals the host via a debug-window slot then waits up to 120 s. |
| `sof llext_list` | `SOF_SHELL_LLEXT_LIST` | List all libraries in IMR/DRAM with DRAM/SRAM mapping state, use count and dependency count. |
| `sof llext_purge <lib_id>` | `SOF_SHELL_LLEXT_PURGE` | Free a library from IMR/DRAM. Fails with `-EBUSY` if any module is still mapped. |

All three require `CONFIG_LIBRARY_MANAGER=y`.

### Host-side usage for `llext_load`

```sh
# Build and sign your library, then:
dd if=my_module.ri \
   of=/sys/kernel/debug/sof/llext_load \
   bs=$(stat -c%s my_module.ri) count=1
```

The firmware prints the result (bytes transferred or error code) on the shell
when the DMA completes.

---

## IPC4 Pipeline Construction Commands (`ppl_create`, `ppl_delete`, `ppl_state`, `mod_init`, `mod_delete`, `mod_bind`, `mod_unbind`)

Gated by `CONFIG_SOF_SHELL_PIPELINE_OPS=y` (default `n`; depends `IPC_MAJOR_4`).
**Intended for bring-up and debug only.** These commands send real IPC4 messages
to the firmware and modify live state; use with care.

| Command | Description |
|---|---|
| `sof ppl_create <id> [prio] [pages] [core] [lp]` | Create an IPC4 pipeline. |
| `sof ppl_delete <id>` | Delete a pipeline and all its module instances. |
| `sof ppl_state <id> <running\|paused\|reset>` | Transition pipeline state. |
| `sof mod_init <mod\|id> <inst> <ppl> [core] [dp]` | Instantiate a module (no init-data blob). |
| `sof mod_delete <mod\|id> <inst>` | Delete a module instance. |
| `sof mod_bind <src\|id> <si> <dst\|id> <di> [sq] [dq]` | Bind src output queue to dst input queue. |
| `sof mod_unbind <src\|id> <si> <dst\|id> <di> [sq] [dq]` | Disconnect two bound modules. |

`mod_init` / `mod_bind` / `mod_unbind` accept the 8-char manifest name (e.g.
`GAIN`) or a numeric `module_id` (decimal or `0x` hex) interchangeably.
Output includes the resolved module name alongside the numeric ID in the format
`NAME(0xID)`.

### Notes

- No IPC4 `init_data` blob is sent by `mod_init`; only modules whose
  firmware defaults work with an empty config will successfully init.
- `ppl_state` calls `ipc4_pipeline_prepare()` then `ipc4_pipeline_trigger()`
  in sequence, mirroring what the host driver does.
- These commands bypass the host driver's topology state machine. Mixing
  shell-driven construction with a running host audio session will likely
  cause errors.

