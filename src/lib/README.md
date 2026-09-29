# Common Library (`src/lib`)

> **Runs in:** Mixed — this directory contains code in more than one tier. `dma.c` and `dai.c`
> host the kernel-side implementations (`z_impl_sof_dma_get/put`, `z_impl_dai_get_device`) of
> syscalls whose validators live in `zephyr/syscall/`; treat them as boundary/kernel code.
> Several other files (`clk.c`, `notifier.c`, `cpu-clk-manager.c`) take Zephyr spinlocks or
> perform privileged platform callbacks and are similarly restricted to kernel context. Truly
> context-agnostic helpers (`lib.c`, `objpool.c`, `ams.c`) can safely be linked into user
> builds. See [User/kernel split](../include/sof/userspace/README.md).

This directory collects cross-cutting helpers that don't belong to a single subsystem. When
adding code here, decide first which tier it belongs to: a pure helper (no locks, no
privileged ops), a privileged-kernel helper (spinlocks, platform callbacks), or a syscall
implementation (kernel-only, reached from user space through a `z_vrfy_` validator). Keep the
tiers in separate files as `dma.c`/`dai.c` and `lib.c`/`objpool.c` already demonstrate.
