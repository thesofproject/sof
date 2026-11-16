# Platform Headers for User-Space Code (`src/platform`)

> **Runs in:** User-space safe — headers only. The platform code is kernel code, in
> [`src/kernel/platform`](../kernel/platform/README.md). See
> [User/kernel split](../include/sof/userspace/README.md).

This directory holds the parts of the platform headers that user-space code may include. It
has the same layout as `src/kernel/platform`, and both are on the include path, so the
headers keep their include names.

Today it has the mailbox:

- `platform/lib/mailbox.h`, and for Intel `ace/lib/mailbox.h` and `cavs/lib/mailbox.h`: the
  mailbox window addresses and the software register accessors, used through
  `sof/lib/mailbox.h`. The software register page and the host box (IPC4) or the stream
  window (IPC3) are mapped into the user-space LL and IPC threads, see
  `user_access_to_mailbox()`.
- `platform/lib/mailbox_layout.h`: the layout of the host memory windows. The platform
  memory map in `src/kernel/platform`, which user-space code must not include, includes it.

The AMD and MediaTek mailbox headers are still in `src/kernel/platform`.
