# Microphone Privacy Manager Architecture

This directory contains the Mic Privacy Manager.

## Overview

Provides a secure mechanism to optionally mute the microphone paths deeply inside the DSP, ensuring user privacy regardless of host OS behavior.

## Configuration and Scripts

- **CMakeLists.txt**: Simply builds the Intel-specific privacy implementation (`mic_privacy_manager_intel.c`). Currently, there is no generic or separate module wrapper defined within the scope of this directory.
