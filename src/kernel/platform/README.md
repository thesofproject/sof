# Platform Support (`src/kernel/platform`)

> **Runs in:** Kernel-only — platform bring-up, memory maps and interrupt wiring are
> supervisor-context concerns and never run in a user thread. The mailbox headers, which
> user-space code uses, are in [`src/platform`](../../platform/README.md). See
> [User/kernel split](../../include/sof/userspace/README.md).

This directory contains SoC/platform-specific integration (e.g. Intel ACE, MediaTek, i.MX),
including the Kconfig that selects the build target such as `PANTHERLAKE` (PTL). It configures
the hardware the audio application later runs on top of, but is not itself part of the
user-space application.
