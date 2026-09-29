# Architecture Support (`src/arch`)

> **Runs in:** Kernel-only — CPU-architecture primitives (Xtensa) execute in supervisor context
> and are never linked into a user thread. See
> [User/kernel split](../include/sof/userspace/README.md).

This directory contains architecture-specific code. It sits below the RTOS and audio
application and has no user-space entry points.
