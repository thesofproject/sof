Read this section if there are some SPDX warnings above.
Pleasing checkpatch is hard when adding new files because:

- checkpatch requests a different SPDX style for .c versus .h files.
  This is because some .h files are included in linker scripts or
  assembly code.
- Some SOF reviewers reject C99 comments starting with //

A trick is to keep the SPDX separate, see solution below.

References:
- https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/process/license-rules.rst#n71
- https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=9f3a89926d6d


Start .h files like this:
```
/* SPDX-License-Identifier: ... */
/*
 * Copyright(c) ...
 *
 * Author: ...
 */
```

Start .c files like this:
```
// SPDX-License-Identifier: ...
/*
 * Copyright(c) ...
 *
 * Author: ...
 */
```
