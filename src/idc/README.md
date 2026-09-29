# Inter-Domain / Inter-Core Communication (`src/idc`)

> **Runs in:** Kernel-only — cross-core messaging (`idc.c`, `zephyr_idc.c`) runs in supervisor
> context. User-space audio code triggers cross-core work indirectly through higher-level
> subsystems, not by calling IDC directly. See
> [User/kernel split](../include/sof/userspace/README.md).

This directory implements the mechanism cores use to signal and hand work to each other. It is
part of the OS-integration layer, not the audio application.
