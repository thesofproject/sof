#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Intel Corporation. All rights reserved.

# Simple helper script for vscode task support.
# Current vscode tasks have difficulty executing multiple commands.

# check if Zephyr environment is set up
if [ ! -z "$ZEPHYR_BASE" ]; then
    VENV_DIR="$ZEPHYR_BASE/.venv"
    echo "Using Zephyr environment at $ZEPHYR_BASE"
elif [ ! -z "$SOF_WORKSPACE" ]; then
    VENV_DIR="$SOF_WORKSPACE/zephyr/.venv"
    echo "Using SOF/Zephyr environment at $SOF_WORKSPACE"
else
    # fallback to the zephyr default from the getting started guide
    VENV_DIR="$HOME/zephyrproject/.venv"
    echo "Using default Zephyr environment at $VENV_DIR"
fi

# start the virtual environment
source ${VENV_DIR}/bin/activate

# The vscode workspace is the parent directory of the SOF Firmware directory
# we need to cd into sof to run the zephyr build script
cd sof

# run the zephyr script
./scripts/xtensa-build-zephyr.py "$@"

# cleanup
deactivate
