# Math Library (`src/math`)

> **Runs in:** Shared library — DSP math (FIR/IIR filters, FFT, trig, decibels, etc.) is pure,
> context-agnostic code linked into whichever thread calls it, including user-space audio
> components. It must stay "user-safe": no privileged operations, no direct hardware or kernel
> object access. See [User/kernel split](../include/sof/userspace/README.md).

This directory provides the numerical routines used by audio components. Because it is pulled
into user-space builds, keep additions free of anything that would require supervisor
privileges; if a routine needs a kernel service, the caller must obtain it through a syscall.
