# Real-Time Noise Reduction (RTNR) Architecture

This directory houses the RTNR component.

## Overview

Reduces steady-state or transient background noise from an input signal path in real time.

## Configuration and Scripts

- **Kconfig**: Dictates the build behavior for the Realtek noise reduction and suppression components (`COMP_RTNR`). The feature depends heavily on specific, proprietary Realtek libraries (`libSOF_RTK_MA_API.a`, etc.). Configures stubs (`COMP_RTNR_STUB`) to circumvent library unavailability during testing and CI logic runs.
- **CMakeLists.txt**: Injects `rtnr.c` directly into Zephyr builds and gracefully falls back to Zephyr external libraries handling. Offers thorough `llext` generation and cleanly wraps stub testing modules `rtnr_stub.c`.
- **rtnr.toml**: Defines topology properties for the loadable RTNR logic (UUID binding, generic pinning setups).
