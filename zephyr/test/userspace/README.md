intel_hda_dma test
------------------

This is a standalone test to exercise the Intel HDA DMA host interface
from a userspace Zephyr thread.
Build with ("ptl" example):

./scripts/xtensa-build-zephyr.py --cmake-args=-DCONFIG_SOF_BOOT_TEST_STANDALONE=y \
        --cmake-args=-DCONFIG_SOF_USERSPACE_INTERFACE_DMA=y \
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
