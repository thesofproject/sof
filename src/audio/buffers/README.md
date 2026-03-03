# Audio Buffers Architecture

This directory contains the core audio buffer management.

## Overview

Buffers connect the output of one component to the input of the next in an audio pipeline graph. They implement circular (ring) buffer semantics and handle cache coherency for DSP memory.

## Architecture Diagram

```mermaid
graph LR
  CompA[Component A / Producer] -->|Write| Buf[Ring Buffer]
  Buf -->|Read| CompB[Component B / Consumer]
```

## Configuration and Scripts

- **CMakeLists.txt**: Includes the base core buffer source files (`audio_buffer.c`, `comp_buffer.c`). When the `CONFIG_PIPELINE_2_0` feature flag is enabled, it additionally compiles `ring_buffer.c`.
- **Topology (.conf)**: Derived from `tools/topology/topology2/include/components/buffer.conf`, defining the `buffer` widget object. Key parameters include `size` (automatically computed), `periods`, `channels`, and `caps` (capabilities like `dai`, `host`, `pass`, `comp`). Defaults to UUID `92:4c:54:42:92:8e:41:4e:b6:79:34:51:9f:1c:1d:28`.
