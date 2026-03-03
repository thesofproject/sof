# Sound Open Firmware (SOF) Architecture

This document provides a high-level overview of the Sound Open Firmware (SOF) architecture. SOF provides an open-source audio DSP firmware and an SDK for audio development.

## 1. High-Level Overview

At a macro level, the SOF stack consists of:
*   **Host Driver (OS):** The OS-level driver (Linux ALSA/ASoC, Windows, etc.) that acts as the frontend, managing audio devices, streams, and IPC communication.
*   **Firmware (DSP):** The embedded firmware running on the Audio DSP (typically Xtensa/Tensilica or other DSPs).
*   **Topology:** A configuration file loaded by the host driver that defines the hardware constraints, routing, and instantiated audio processing graphs (pipelines).

## 2. Firmware Architecture

The SOF firmware is built with a modular, layered architecture to support multiple platforms, DSP architectures, and operating systems.

### 2.1 OS Abstraction and RTOS
*   **Zephyr RTOS:** Modern SOF extensively leverages the Zephyr RTOS for core operating system services, including thread scheduling, synchronization, logging, and hardware abstraction.
*   **XTOS / Bare-metal (Legacy):** Older platforms or specific stripped-down environments might use XTOS or bare-metal abstractions.

### 2.2 Inter-Process Communication (IPC)
The IPC subsystem is responsible for all communication between the Host OS and the DSP Firmware.
*   **Message Passing:** Uses hardware mailboxes, shared memory, and interrupts to send messages.
*   **Commands:** Includes stream setup, pipeline instantiation, run/pause/stop commands, volume changes, and parameter updates.
*   **Versions:** IPC relies on a versioned message protocol (e.g., IPC3, IPC4) allowing the host and firmware to understand each other's commands.

### 2.3 Audio Processing Pipelines
At the core of the SOF firmware is the DSP pipeline. A pipeline is a directed graph consisting of connected audio modules (components) that process streams of data.
*   **Endpoints:** Pipelines connect external interfaces (like I2S/HDA/DMIC DAIs - Digital Audio Interfaces) to host DMA buffers.
*   **Scheduling:** Each pipeline is scheduled to run periodically based on its timing requirements and deadlines. SOF provides an LL (Low Latency) scheduler and an EDF (Earliest Deadline First) scheduler.

### 2.4 Audio Components (Modules)
The firmware processing graph is assembled using individual audio modules, which are dynamically instantiated based on the topology file:
*   **Host / DAI Components:** Read/write audio frames from/to DMA buffers or hardware interfaces.
*   **Copier:** Moves data between buffers or other components without modification.
*   **Volume / Mixer:** Changes signal amplitude or mixes multiple streams.
*   **SRC (Sample Rate Converter):** Resamples audio streams.
*   **Effect Modules:** EQs (Equalizers), DRCs (Dynamic Range Compressors), smart amplifiers, arrays, and other algorithmic components.

### 2.5 Memory Management
The DSP handles multiple memory domains depending on the hardware (SRAM, L1 Cache, L2, external memory).
*   **Heaps:** Different memory heaps exist for dynamic allocation (e.g., system heap, buffer heap, runtime heap) to ensure real-time constraints and avoid fragmentation.
*   **Data Buffers:** Audio buffers are carefully allocated in appropriate memory to minimize power consumption and pipeline latency.

## 3. Topologies

The host driver parses a topology file (`.tplg`) which describes the expected processing graphs. Instead of hardcoding audio routing in the firmware, SOF uses these topology constraints. The firmware receives the component creation sequences, parameter configurations, and connections over IPC to construct pipelines at runtime.
