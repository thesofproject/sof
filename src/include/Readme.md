** SOF Header Layout **

Today SOF is somewhat coupled with xtensa xtos/HAL and it's own SOF RTOS logic.
This  coupling is reflected in the headers where we have a mix of RTOS and
common headers at the top level ```src/include``` directory.

Previously SOF had the following **mandatory** include PATHS

 1) Toplevel ```src/include```

 The top level common which included RTOS level APIs, drivers, library, IPC, debug and audio.

 2) Platform ```$PLATFORM```

 Platform level includes for IP configuration and IP drivers.

 3) Architecture ```$ARCH```

 DSP Architecture level includes that define DSP architecture.


 ** RTOS Header Decoupling **

 Due to the mixing of RTOS, driver and library headers at the top level include
 directory it was necessary to create RTOS specific directories for RTOS specific
 logic and headers i.e.

```
xtos/include
zephyr/include
```

These RTOS include directories will eventually contain RTOS specific headers
whilst common logic and structures will be placed in non RTOS directories.

This will also mean

```c
#include <sof/spinlock.h>
```

will become

```c
#include <rtos/spinlock.h>
```

and will allow easier visualisation of where and why RTOS headers are being used.
This will help to eliminate cross usage of headers between RTOSes.