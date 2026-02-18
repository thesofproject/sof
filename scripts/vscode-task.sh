#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Intel Corporation. All rights reserved.

# Simple helper script for vscode task support.
# Current vscode tasks have difficulty executing multiple commands.

# check if SOF workspace environment is set up
if [ ! -z "$SOF_WORKSPACE" ]; then
    VENV_DIR="$SOF_WORKSPACE/.venv"
    echo "Using SOF environment at $SOF_WORKSPACE"
else
    # fallback to the script directory default path
    SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
    SOF_REPO=$(dirname "$SCRIPT_DIR")
    VENV_DIR="$SOF_REPO/../.venv"
    echo "Using default SOF environment at $VENV_DIR"
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
