# Audio Components (`src/audio`)

> **Runs in:** Application — under userspace configs (e.g. PTL with `CONFIG_SOF_USERSPACE_LL`),
> the pipeline, buffers, copiers and processing components in this tree run in MMU-isolated
> **user** threads. They reach kernel resources only through the SOF syscalls. See
> [User/kernel split](../include/sof/userspace/README.md).

This directory holds the SOF audio application: the pipeline graph and scheduling glue, host/DAI
copiers, and the ~36 processing components/modules (volume, EQ, SRC, mixers, codecs, etc.). This
is the code the user/kernel split is designed to protect the rest of the firmware from.

Notable subdirectories carry their own README with a matching "Runs in:" banner, e.g.
[`module_adapter/`](module_adapter/README.md), which is the boundary container that launches
modules in user context.

When adding a component here, keep it inside the application tier: allocate through the module
allocation syscalls (`mod_*`), mark any globals shared with the kernel using the partition
markers, and never call kernel-only subsystems (`drivers/`, `platform/`, `idc/`) directly.
