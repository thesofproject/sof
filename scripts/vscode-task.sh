#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Intel Corporation. All rights reserved.

# Simple helper script for vscode task support.
# Current vscode tasks have difficulty executing multiple commands.

# setup environment
source ~/zephyrproject/.venv/bin/activate

# vscode workspace root is the parent directory of sof
# we need to cd into sof to run the zephyr build script
cd sof

# run the zephyr script
./scripts/xtensa-build-zephyr.py "$@"

# cleanup
deactivate
