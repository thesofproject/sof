# SOF on top of Zephyr

### West

Zephyr folder contains a west [module](https://docs.zephyrproject.org/latest/guides/modules.html)

### Sources in 'zephyr/sof-wrapper'

This directory contains wrapped implementations of generic SOF OS services,
implementing the public SOF interface, but using Zephyr OS for 
the implementation.

### Porting

At current stage this effort is not trying to reconsile APIs from Zephyr with
SOF, although that may be possible that would require reworking a few key
areas, here is a few points that are not straight forward:

1. Board + device tree vs platform header: Zephyr makes use of board
   definitions plus device trees to describe the memory areas, buses,
   peripherals, etc, while SOF has a platform header which serves for the same
   purpose.

2. Static vs dynamic allocation: Zephyr is tuned to have static allocation to
   minimize the RAM usage as much as possible, SOF in the other hand takes a
   more traditional approach so most of its APIs end up allocating objects
   dynamically.

3. Indirect vs direct waiters: SOF has dedicated APIs to wait for conditions,
   events, while Zephyr embed this sort of mechanism into its APIs so calls to
   them can make a thread wait for some condition.
