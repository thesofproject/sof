# Common Library (`src/lib`)

> **Runs in:** Mixed — this directory contains code in more than one tier. `dma.c` and `dai.c`
> host the kernel-side implementations (`z_impl_sof_dma_get/put`, `z_impl_dai_get_device`) of
> syscalls whose validators live in `zephyr/syscall/`; treat them as boundary/kernel code.
> `notifier.c` takes Zephyr spinlocks and is similarly restricted to kernel context. The clock,
> KCPS and AMS code (`clk.c`, `cpu-clk-manager.c`, `ams.c`) is kernel-only and lives in
> `src/kernel/lib`. Truly context-agnostic helpers (`lib.c`, `objpool.c`) can safely be linked
> into user builds. See [User/kernel split](../include/sof/userspace/README.md).

This directory collects cross-cutting helpers that don't belong to a single subsystem. When
adding code here, decide first which tier it belongs to: a pure helper (no locks, no
privileged ops), a privileged-kernel helper (spinlocks, platform callbacks), or a syscall
implementation (kernel-only, reached from user space through a `z_vrfy_` validator). Keep the
tiers in separate files as `dma.c`/`dai.c` and `lib.c`/`objpool.c` already demonstrate.
