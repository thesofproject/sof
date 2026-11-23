User-space interface tests for Intel ADSP
-----------------------------------------

This folder contains multiple tests to exercise Intel DSP device interfaces
from a user-space Zephyr thread.

Available tests:
- test_intel_hda_dma.c
   - Test Intel HDA DMA host interface from a userspace
     Zephyr thread. Use cavstool.py as host runner.
- test_intel_ssp_dai.c
   - Test Zephyr DAI interface, together with SOF DMA
     wrapper from a user thread. Mimics the call flows done in
     sof/src/audio/dai-zephyr.c. Use cavstool.py as host runner.
- test_ll_task.c
   - Test Low-Latency (LL) scheduler in user-space mode. Creates
     a user-space LL scheduler, and uses it to create and run tasks.
   - Tests functionality used by SOF audio pipeline framework to
     create tasks for audio pipeline logic.
- test_mailbox.c
   - Test use of sof/mailbox.h interface from a Zephyr user thread.
- test_ll_user_heap.c
   - Test the LL user heap, whose allocator runs in user mode. Checks
     that kernel code refuses to use it, that a user thread corrupting
     its metadata only faults user threads, and that the DP module
     resource system calls reject LL modules and forged contexts.
   - Also tests the fast_get area of the LL memory region: LL user
     threads get shared read-only copies, a freed copy is reused
     without stale data, only the LL and IPC user threads may get
     copies of data they can read, and LL modules fall back to a
     private copy when the area is full.

Building for Intel Panther Lake:
./scripts/xtensa-build-zephyr.py --cmake-args=-DCONFIG_SOF_BOOT_TEST_STANDALONE=y \
        --cmake-args=-DCONFIG_SOF_USERSPACE_INTERFACE_DMA=y \
        --cmake-args=-DCONFIG_SOF_USERSPACE_LL=y \
        -o app/internal_ci_overlay.conf \
        -o app/overlays/ptl/userspace_overlay.conf -o app/winconsole_overlay.conf ptl

Running test:
- Copy resulting firmware (sof-ptl.ri) to device under test.
- Boot and run the test with cavstool.py:
  sudo ./cavstool.py sof-ptl.ri
- Test results printed to cavstool.py

Running test on QEMU (dc233c MMU):
- Tests can also be built and run locally using Zephyr's QEMU simulator.
- First, build the test application using `xtensa-build-zephyr.py`:
  ./scripts/xtensa-build-zephyr.py qemu_xtensa_mmu --cmake-args=-DCONFIG_SOF_BOOT_TEST_STANDALONE=y \
      --cmake-args=-DCONFIG_SOF_USERSPACE_INTERFACE_DMA=y --cmake-args=-DCONFIG_SOF_USERSPACE_LL=y
- Once built, run the test in QEMU:
  west build -d build-qemu_xtensa_mmu -t run

References to related assets in Zephyr codebase:
- cavstool.py
    - zephyr/soc/intel/intel_adsp/tools/cavstool.py
- HD DMA tests in Zephyr
    - zephyr/tests/boards/intel_adsp/hda/
    - larger set in kernel space, using DMA interface directly without SOF dependencies
