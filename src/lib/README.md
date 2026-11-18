# Common Library (`src/lib`)

> **Runs in:** Mixed — this directory contains code in more than one tier. `dma.c` (the DMA
> buffer helpers) and `dai.c` (the DAI groups) also run in the user-space LL thread. Their
> kernel parts, including the kernel-side implementations (`z_impl_sof_dma_get/put`,
> `z_impl_dai_get_device`) of syscalls whose validators live in `zephyr/syscall/`, are in
> `src/kernel/lib/dma-kernel.c` and `dai-kernel.c`. `notifier.c` takes Zephyr spinlocks and is
> restricted to kernel context. The clock, KCPS and AMS code (`clk.c`, `cpu-clk-manager.c`,
> `ams.c`) is kernel-only and lives in `src/kernel/lib`. Truly context-agnostic helpers
> (`lib.c`, `objpool.c`) can safely be linked into user builds. See [User/kernel
> split](../include/sof/userspace/README.md).

This directory collects cross-cutting helpers that don't belong to a single subsystem. When
adding code here, decide first which tier it belongs to: a pure helper (no locks, no
privileged ops), a privileged-kernel helper (spinlocks, platform callbacks), or a syscall
implementation (kernel-only, reached from user space through a `z_vrfy_` validator). Keep the
tiers in separate files: kernel code goes to `src/kernel/lib`, as for `dma.c` and `dai.c`.
