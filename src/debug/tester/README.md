# Tester Component

The `tester` directory implements a pseudo-component inside the SOF architecture. Instead of processing audio like an EQ or Mixer, it acts as a test execution harness designed specifically for Continuous Integration (CI) and automated system verifications.

## Feature Overview

Continuous integration often requires validating low-level system interactions that cannot be reached or easily triggered via standard ALSA driver behaviors. To overcome this, the `tester` component can be instantiated via Topologies or explicit IPC commands. Once bound, the host can send configuration blobs triggering embedded testing subroutines compiled directly into the firmware.

Currently, it supports functionalities like:

* **Dummy Operations**: Echoing parameters to verify full-stack serialization roundtrips.
* **DRAM Verification**: Stress-testing memory allocation arrays within the `LLB`/`L2` domains to detect caching bugs or out-of-bounds pointer crashes (`tester_simple_dram_test`).

## Architecture

The architecture functions identical to an IPC Version 4 standard module, containing `comp_driver`, `comp_dev`, `comp_buffer` wrappers and implementing `tester_params()` handlers.

```mermaid
graph TD
    subgraph Host Automation
        Testbench[CI Testbench / ALSA Tool]
        Testbench -->> |Topology Payload: CMD_TEST| IPC
    end

    subgraph SOF Firmware
        IPC[IPC4 Handler] -->> |ipc4_set_module_params| Tester[Tester Component]

        Tester -->> |Switch on Test Type| Dummy[tester_dummy_test.c]
        Tester -->> |Switch on Test Type| DRAM[tester_simple_dram_test.c]

        Dummy -->> IPC
        DRAM -->> |Runs Data Verifications| ExtMemory[SoC DRAM / Caches]
        DRAM -->> IPC
    end
```

## How to Enable

Because the tester artificially exposes internal memory bounds limits and allows arbitrary firmware injection tests, it is locked out of standard release payloads.

To compile it into your build, you must append:

* `CONFIG_COMP_TESTER=y`

**Constraints:**
It strictly `depends on IPC_MAJOR_4`. You cannot instantiate this tester under the legacy IPC3 streaming mechanisms.

## Creating New Tests

Developers diagnosing tricky hardware bugs can extend the tester by:

1. Adding a new case to the test selection switch in `tester.c:tester_init()` (and, if needed, extending runtime handling in `tester_set_configuration()`).
2. Defining a new `test_runner()` C file implementing their data patterns.
3. Adding the target configurations in `tester.toml` to automatically sync with the module offset calculators (`llext_offset_calc`).
