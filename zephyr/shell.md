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

### 2. `sof module_heap_usage`
- **Description**: Dumps heap memory usage for all active audio modules.
- **Usage**: `sof module_heap_usage`
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

---

## Enabling Shell Commands

Ensure the following Kconfig symbols are enabled in your build configuration:
- `CONFIG_SHELL=y`
- `CONFIG_SOF_VREGIONS=y` (for `vpage` and `vregion` commands)

---

## Functional Areas Missing Shell Coverage

Tracking list of subsystems that lack runtime shell visibility for control,
debug or testing. Items get ticked off as commands land on `topic/shell`.

### Control

| Area | Suggested commands | Status |
|---|---|---|
| kcontrols / mixer | `kctl_list`, `kctl_get <id>`, `kctl_set <id> <val>` | TODO |
| Module runtime config | `mod_config_get/set <mod_id> <inst> <param_id>` | TODO |
| Stream / copier | `stream_list`, `stream_pause/resume <id>`, `copier_gain_set` | TODO |
| Clocks (extend `clock_status`) | `clock_set <core> <freq>`, `clock_force <pll>` | TODO |
| Power management | `pm_state`, `pm_force <state>`, `pg_status`, `idle_stats` | TODO |
| Cache | `dcache_flush <addr> <size>`, `dcache_inv`, `icache_inv` | TODO |
| Watchdog | `wdt_status`, `wdt_kick`, `wdt_disable` | TODO |
| DAI / link control | `dai_list`, `dai_status <type> <idx>`, `dai_trigger`, `dai_loopback` | TODO |
| DMA | `dma_list`, `dma_chan_status <dma> <chan>`, `dma_stop` | TODO |

### Debug

| Area | Suggested commands | Status |
|---|---|---|
| **IPC counters / last message** | `ipc_stats`, `ipc_last` | **DONE (task 1)** |
| IPC inject | `ipc_inject <hex>`, `ipc_queue` | TODO |
| Audio buffers | `buffer_list`, `buffer_info <id>` | **DONE (task 2)** |
| **Scheduler** | `sched_tasks`, `sched_load`, `task_info <task>` | **DONE (task 3)** |
| Logging / trace | `log_level <component> <lvl>`, `log_filter`, `mtrace_dump`, `trace_stats` | TODO |
| Telemetry / perf | `perf_status`, `perf_reset`, `cpu_load` | TODO |
| Notifications | `notify_subscribers`, `notify_stats` | TODO |
| Debug window / mailbox | `dbgwin_dump <slot>`, `mailbox_hex` | TODO |
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

1. **`ipc_stats` / `ipc_last`** &mdash; DONE.
2. **`buffer_list` / `buffer_info`** &mdash; DONE.
3. **`sched_tasks` / `sched_load`** &mdash; DONE.
4. `log_level` / `mtrace_dump` &mdash; runtime log tuning without rebuild.
5. `crash_log` / `bt` &mdash; pair with the `crash-*` artifacts in the tree.
6. `perf_status` &mdash; wraps SOF telemetry counters.
7. `dai_list` / `dma_chan_status` &mdash; link/DMA blind spot.
8. `kctl_get/set` &mdash; today only doable via tplg/IPC.

---

## Task 1 &mdash; `ipc_stats` / `ipc_last`

### Commands

| Command | Description |
|---|---|
| `sof ipc_stats` | Print RX/TX counters (`rx_count`, `rx_errors`, `tx_count`, `tx_direct_count`, `tx_errors`). |
| `sof ipc_stats reset` | Clear all counters. |
| `sof ipc_last` | Print the most recent RX and TX `pri`/`ext` headers and their platform-cycle timestamps. |

### Implementation

- Public API in `src/include/sof/ipc/common.h`:
  - `struct ipc_stats`
  - `ipc_stats_record_rx(pri, ext)`
  - `ipc_stats_record_tx(pri, ext, direct, err)`
  - `ipc_stats_inc_rx_error()`
  - `ipc_stats_get(out)` / `ipc_stats_reset()`
- Storage and locking in `src/ipc/ipc-common.c` (single global, protected by
  `ipc->lock`, IPC-version agnostic).
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

## Task 2 &mdash; `buffer_list` / `buffer_info`

### Commands

| Command | Description |
|---|---|
| `sof buffer_list` | List every audio buffer in the pipeline with source/sink component IDs, size, avail, free, channels, rate and frame format. |
| `sof buffer_info <buffer_id>` | Detailed info for a single buffer: source/sink comps, core, flags, size/avail/free bytes, rptr, wptr, channels, rate, frame format. |

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
- `buffer_info` does not yet decode `flags` symbolically.
- A future enhancement could add `--core <n>` filtering and per-buffer
  topology graph output.

## Task 3 &mdash; `sched_tasks` / `sched_load`

### Commands

| Command | Description |
|---|---|
| `sof sched_tasks` | List every task across all SOF schedulers (LL timer, LL DMA, EDF, DP, TWB) with type, core, priority, state, flags and uid. |
| `sof sched_load`  | Per-task cycle counters (cycles_cnt, cycles_sum, cycles_max, derived average) plus aggregate totals. Pairs with `test_inject_sched_gap`. |

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
