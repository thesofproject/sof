# Platform Support (`src/kernel/platform`)

> **Runs in:** Kernel-only — platform bring-up, memory maps and interrupt wiring are
> supervisor-context concerns and never run in a user thread. The mailbox headers, which
> user-space code uses, are in [`src/platform`](../../platform/README.md). See
> [User/kernel split](../../include/sof/userspace/README.md).

This directory contains SoC/platform-specific integration (e.g. Intel ACE, MediaTek, i.MX),
including the Kconfig that selects the build target such as `PANTHERLAKE` (PTL). It configures
the hardware the audio application later runs on top of, but is not itself part of the
user-space application.

## Platform header contract

Generic code reaches a platform through fixed include names. The build puts
`src/kernel/platform/<platform>/include` and `src/platform/<platform>/include` on the include
path, plus any shared directories (`intel/ace`, `intel/cavs`, `amd/common`). A platform only
has to provide what the generic code uses:

| Include name | Included by | What generic code uses |
|---|---|---|
| `platform/platform.h` | `sof/kernel/platform.h` | `HOST_PAGE_SIZE`, `PLATFORM_PAGE_TABLE_SIZE`, `HW_CFG_VERSION`; `platform_wait_for_interrupt()`, `ipc_platform_send_msg()`, `ipc_platform_send_msg_direct()` |
| `platform/lib/memory.h` | `sof/lib/memory.h` | `PLATFORM_DCACHE_ALIGN`, `HOST_PAGE_SIZE`, `SOF_STACK_SIZE`, `host_to_local()`, `local_to_host()`; where the platform has them: the heap sizes (`HEAPMEM_SIZE`, `HEAP_*_SIZE`), `SRAM_BANK_SIZE`, `LP_SRAM_SIZE`, `IMR_BOOT_LDR_MANIFEST_BASE`, `DMA0_BASE`/`DMA0_SIZE`, `MU_BASE` |
| `platform/lib/clk.h` | `rtos/clk.h` | `NUM_CLOCKS`, `NUM_CPU_FREQ`, `CPU_DEFAULT_IDX`, `CPU_LOWEST_FREQ_IDX`, `CLK_MAX_CPU_HZ`, `CLK_SSP`, `PRIMARY_CORE_BASE_CPS_USAGE`, `SECONDARY_CORE_BASE_CPS_USAGE` |
| `platform/lib/mailbox.h`, `platform/lib/mailbox_layout.h` (in `src/platform`) | `sof/lib/mailbox.h` | the host windows `MAILBOX_{DSPBOX,HOSTBOX,STREAM}_{BASE,SIZE}`, `MAILBOX_DEBUG_BASE`, `MAILBOX_SW_REG_{BASE,SIZE}`, and where used `SRAM_OUTBOX_SIZE`, `SRAM_STREAM_BASE`; `mailbox_sw_reg_read()`, `mailbox_sw_reg_read64()`, `mailbox_sw_reg_write()`, `mailbox_sw_regs_write()` |
| `platform/lib/dma.h` | `sof/lib/dma.h`, only for the legacy DMA domain schedulers | `PLATFORM_NUM_DMACS`, `PLATFORM_MAX_DMA_CHAN`, `dma_chan_irq()`, `dma_chan_irq_name()` |
| `platform/lib/dai.h` | `sof/lib/dai-legacy.h`, only without `CONFIG_ZEPHYR_NATIVE_DRIVERS` | nothing |
| `platform/drivers/interrupt.h` | `rtos/interrupt-legacy.h`, only for AMD without native drivers | `PLATFORM_IRQ_HW_NUM`, `PLATFORM_IRQ_CHILDREN`, `PLATFORM_IRQ_FIRST_CHILD` |

The host builds (host library, testbench, cmocka) use the `library` platform through the
`posix/include` wrappers. Only those builds need `platform/lib/cpu.h`
(`PLATFORM_PRIMARY_CORE_ID`), `platform/drivers/idc.h` (`idc_init()`, `idc_send_msg()`),
`platform/lib/ll_schedule.h` and the host allocator's `PLATFORM_HEAP_*` counts.

A new platform does not need any other header or macro. In particular it does not need
`platform/trace/trace.h`, `platform/lib/pm_runtime.h`, the DMA trace settings or an XTOS memory
layout.

`scripts/check_platform_headers.py` lists platform headers that nothing can include and
platform macros that nothing can use, from the source tree. `--contract` prints the table above
from the current code. CI runs it with `--strict`: the known findings are listed in the
`[baseline]` of `scripts/platform_headers.txt`, which may only shrink, so new unused headers and
macros fail. A new platform whose builds use more than its own directory, or that has vendor
drivers outside it, is listed in the `[groups]` and `[vendor]` sections of the same file.
