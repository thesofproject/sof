#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Intel Corporation. All rights reserved.

# Define the build directory from the first argument (or default)
BUILD_DIR="${1:-build}"

# Find and source the zephyr environment script, typically via the sof-venv wrapper
# or directly if running in a known zephyrproject layout.
# We will use the existing helper sof-venv.sh to get the right environment.

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOF_WORKSPACE="$(dirname "$(dirname "$SCRIPT_DIR")")"

# check if Zephyr environment is set up
if [ ! -z "$SOF_WORKSPACE" ]; then
    VENV_DIR="$SOF_WORKSPACE/.venv"
    echo "Using SOF environment at $SOF_WORKSPACE"
else
    # default to the local workspace
    VENV_DIR="${SOF_WORKSPACE}/.venv"
    echo "Using default SOF environment at $VENV_DIR"
fi

# start the virtual environment
source ${VENV_DIR}/bin/activate

# Execute the QEMU runner from within the correct build directory
cd "${BUILD_DIR}" || exit 1

# Finally run the python script which will now correctly inherit 'west' from the sourced environment.
python3 "${SCRIPT_DIR}/sof-qemu-run.py" --build-dir "${BUILD_DIR}"

