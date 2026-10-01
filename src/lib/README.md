# Common Library (`src/lib`)

> **Runs in:** Shared library — **with a boundary exception.** Most of this directory
> (`lib.c`, `notifier.c`, `objpool.c`, `clk.c`, `agent.c`, `ams.c`, `cpu-clk-manager.c`) is
> context-agnostic helper code linked into whichever thread calls it and must stay user-safe.
> However, `dma.c` and `dai.c` host the **kernel-side implementations** (`z_impl_sof_dma_*`,
> `z_impl_dai_get_device`) of syscalls whose validators live in `zephyr/syscall/`. Treat those
> two files as boundary/kernel code. See
> [User/kernel split](../include/sof/userspace/README.md).

This directory collects cross-cutting helpers that don't belong to a single subsystem. When
adding code here, decide first whether it is shared-library (user-safe, no privileged ops) or a
syscall implementation (kernel-only, reached from user space through a `z_vrfy_` validator), and
keep the two kinds in separate files as `dma.c`/`dai.c` already do.
