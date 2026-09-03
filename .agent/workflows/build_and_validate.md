---
description: Build and validate new C code features
---

This workflow describes the process for building and validating any new C code features in the SOF repository.

**Note:** The QEMU build targets must be used for both building and testing. The user requires the build must be error and warning free and the ztests must all pass.

// turbo-all
1. Build the new C code feature using the `xtensa-build-zephyr.py` script.
    ```bash
    source ../.venv/bin/activate
    ./scripts/xtensa-build-zephyr.py qemu_xtensa
    ```

2. Validate the feature with a ztest run using the `sof-qemu-run.sh` script.
    ```bash
    source ../.venv/bin/activate
    ./scripts/sof-qemu-run.sh build-qemu_xtensa
    ```

3. Ensure that all new features and functions have appropriate Doxygen comments and that the Doxygen documentation builds without errors or warnings.