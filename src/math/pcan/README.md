# Per-Channel AGC Normalization (PCAN) Math Library

This directory contains the **PCAN (Per-Channel Automatic Gain Control Normalization)** library for Sound Open Firmware (SOF), integrating Google's upstream `microfrontend` library from [TFLite Micro](https://github.com/tensorflow/tflite-micro/tree/main/tensorflow/lite/experimental/microfrontend/lib) under the **Apache 2.0 license** with express patent grant protection.

---

## 1. Overview & Mathematical Formulation

PCAN applies per-channel adaptive dynamic gain control and root dynamic range compression across time to linear filterbank energies (e.g. Mel spectrogram bins), suppressing stationary background noise while enhancing transient acoustic events (speech onsets, keywords).

```
                  +-------------------------------------------------------------+
                  |               Input Filterbank Energy Matrix                |
                  |                   E_in[channel, frame]                      |
                  +-------------------------------------------------------------+
                                                 |
                                                 v
+---------------------------------------------------------------------------------------------+
|                                  PCAN Processing Stages                                     |
|                                                                                             |
|  1. Temporal Noise Estimation (IIR Smoothing):                                              |
|     E_est[k] = ((E_in[k] << smoothing_bits)*alpha_s + E_est[k]*(1 - alpha_s)) >> 14         |
|                                                                                             |
|  2. Piecewise Quadratic Octave Gain Lookup:                                                 |
|     gain[k] = WideDynamicFunction(E_est[k], gain_lut)                                       |
|               - Fast single-cycle CLZ/MSB (AE_NSAU on Tensilica HiFi)                       |
|               - 10-bit fractional index quadratic interpolation                             |
|                                                                                             |
|  3. Dynamic Gain & Root Polynomial Compression:                                             |
|     SNR[k] = ((uint64_t)E_in[k] * gain[k]) >> snr_shift                                     |
|     E_out[k] = PcanShrink(SNR[k])                                                           |
|                - If SNR[k] < 8192 (2 << 12):  SNR[k]^2 >> 20                                |
|                - If SNR[k] >= 8192:           (SNR[k] >> 6) - 64                            |
+---------------------------------------------------------------------------------------------+
                                                 |
                                                 v
                  +-------------------------------------------------------------+
                  |                PCAN-Normalized Feature Tensor               |
                  |                   E_out[channel, frame]                     |
                  +-------------------------------------------------------------+
```

---

## 2. Dependencies

The PCAN math library requires the following tools and repositories:

| Dependency | Required Version | Description |
|---|---|---|
| **West Manifest (`west.yml`)** | $\ge 0.13$ | Pulls `tflite-micro` workspace dependency via `west update` |
| **TFLite-Micro** | Git revision `e86d97b6` | Upstream Google microfrontend C source files (`pcan_gain_control.c`, `noise_reduction.c`, `bits.h`) |
| **CMake** | $\ge 3.13$ | SOF build and configuration system |
| **Host Toolchain** | GCC $\ge 9.0$ / Clang $\ge 11.0$ | For building host testbench and CMocka unit tests |
| **DSP Toolchains** | Zephyr SDK / Xtensa XCC / Clang | For building target firmware with HiFi3 / HiFi4 / HiFi5 intrinsics |
| **CMocka** | Built with SOF | Unit testing framework for bit-exact validation |
| **GNU Octave** | $\ge 5.0$ | Reference modeling and golden test vector generation |

---

## 3. Build & Test Instructions

### 3.1 Synchronize Workspace via West
Ensure `tflite-micro` is pulled and synchronized in your workspace:
```bash
cd /path/to/sof-workspace
west update
```

### 3.2 Build and Run CMocka Unit Tests (Host)
Configure and run the 5-phase PCAN unit test suite on the host:
```bash
# Configure unit test build
cmake -S sof -B build_ut -DBUILD_UNIT_TESTS=ON -DBUILD_UNIT_TESTS_HOST=ON -DINIT_CONFIG=unit_test_defconfig

# Build PCAN unit test binary
cmake --build build_ut --target pcan

# Run PCAN test executable directly
./build_ut/test/cmocka/src/math/pcan/pcan

# Or run via CTest with full math regression
ctest --test-dir build_ut -R "pcan|auditory|dct|matrix|window|fft" --output-on-failure
```

### 3.3 Regenerate Octave Golden Reference Vectors
To re-generate or verify golden test headers from the Octave reference suite:
```bash
cd test/cmocka/src/math/pcan
octave-cli --eval "ref_pcan"
```
This updates `ref_pcan_lut.h`, `ref_pcan_func.h`, `ref_pcan_stream.h`, and `ref_pcan_corners.h`.

### 3.4 Build Target Firmware (Zephyr / SOF)
When building SOF for a target DSP platform (e.g. Intel ACE15 / Meteor Lake / Arrow Lake):
```bash
west build -b intel_adsp_ace15_mtpm app
```

---

## 4. Kconfig & CMake Options

- `CONFIG_MATH_PCAN`: Enables building the PCAN math library and links Google's microfrontend C source files.
- `CONFIG_PCAN_HIFI3` / `CONFIG_PCAN_HIFI4`: Enables Tensilica HiFi DSP assembly/SIMD optimizations.
- `CONFIG_COMP_MFCC`: Automatically selects `CONFIG_MATH_PCAN` when MFCC feature extraction is enabled.
