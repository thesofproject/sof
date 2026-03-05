# PCM Format Converter Architecture

This directory provides PCM format conversion.

## Overview

Handles conversions like 16-bit to 24-bit, interleaved to non-interleaved, and other basic PCM layout translations.

## Configuration and Scripts

- **CMakeLists.txt**: Organizes standard and highly performant HIFI implementations (`pcm_converter.c`, `pcm_converter_generic.c`, `pcm_converter_hifi3.c`). Includes optional support for `pcm_remap.c` dependent on the `CONFIG_PCM_REMAPPING_CONVERTERS` flag.
