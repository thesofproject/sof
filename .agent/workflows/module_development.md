---
description: Develop and validate new audio processing modules
---

This workflow describes the expected steps to create and validate a new audio processing module within the SOF repository.

// turbo-all
1. **(Optional)** Generate the module skeleton using the `sdk-create-module.py` script.
    ```bash
    # Run the script with relevant arguments to create a new module template
    ./scripts/sdk-create-module.py --name <module_name> --version <version>
    ```

2. Develop the module logic within the generated skeleton.

3. Validate the module by executing the module within the host testbench. This ensures that the module functions as expected outside of full system simulations.
    ```bash
    # Configure and run the testbench against the developed module
    ./scripts/host-testbench.sh -l <path_to_module_library>
    ```

4. Document the new module using Doxygen comments. Validate that the Doxygen build completes without errors or warnings. Add a README.md for the module.