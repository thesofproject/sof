# Kernel-only code (`src/kernel`)

> **Runs in:** Kernel-only — everything here runs in supervisor context and never executes in
> a user thread. User code reaches it only through system calls. See
> [User/kernel split](../include/sof/userspace/README.md).

This directory collects the SOF code that only runs in kernel mode: OS, hardware and
memory-domain facing code such as boot and initialization, inter-core communication and
module loading. Kernel-only code elsewhere in `src/` moves here one subsystem at a time as part
of the kernel / user-space partition.

## Layout

Each subdirectory keeps the path the code had under `src/`, so `src/<path>` moves to
`src/kernel/<path>`. For example `src/audio/base_fw.c` becomes `src/kernel/audio/base_fw.c`.
When only some files of a directory are kernel-only, only those files move, and the rest of
the directory stays where it is.

## Build

Zephyr builds everything below this directory into the `sof_kernel` library instead of
`modules_sof`. [`CMakeLists.txt`](CMakeLists.txt) makes `sof_kernel` the current Zephyr
library for this directory tree, so the CMake files of moved directories do not change. Their
Kconfig files are sourced from [`Kconfig`](Kconfig).

Code here may use kernel-only headers. User-space code must not call into it directly; the
checker in `scripts/check_kernel_includes.py` reports kernel-only includes from user-space
sources.
