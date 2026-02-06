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

Building for Intel Panther Lake:
./scripts/xtensa-build-zephyr.py --cmake-args=-DCONFIG_SOF_BOOT_TEST_STANDALONE=y \
        --cmake-args=-DCONFIG_SOF_USERSPACE_INTERFACE_DMA=y \
        --cmake-args=-DCONFIG_SOF_USERSPACE_LL=y \
        -o app/overlays/ptl/userspace_overlay.conf -o app/winconsole_overlay.conf ptl

Running test:
- Copy resulting firmware (sof-ptl.ri) to device under test.
- Boot and run the test with cavstool.py:
  sudo ./cavstool.py sof-ptl.ri
- Test results printed to cavstool.py

References to related assets in Zephyr codebase:
- cavstool.py
    - zephyr/soc/intel/intel_adsp/tools/cavstool.py
- HD DMA tests in Zephyr
    - zephyr/tests/boards/intel_adsp/hda/
    - larger set in kernel space, using DMA interface directly without SOF dependencies
