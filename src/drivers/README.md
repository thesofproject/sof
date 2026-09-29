# Device Drivers (`src/drivers`)

> **Runs in:** Kernel-only — hardware/HAL access runs in supervisor context and is never
> executed from a user thread. User-space audio code reaches driver functionality only through
> SOF syscalls (e.g. the `sof_dma_*` and `dai_get_device` calls). See
> [User/kernel split](../include/sof/userspace/README.md).

This directory contains low-level device drivers and hardware abstraction. Because it touches
MMIO, interrupts and other privileged resources, nothing here is mapped into a user memory
domain. If user-space code needs a driver operation, expose it through a validated syscall in
the boundary tier rather than linking this code into a user thread.
