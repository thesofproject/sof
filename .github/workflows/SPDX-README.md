Pleasing checkpatch is hard when adding new files because:

- checkpatch wants a different SPDX style for .c versus .h files!
- SOF rejects C99 comments starting with //

The trick is to keep the SPDX separate. See solution below.

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
