#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.

# Parse arguments to determine the build directory
BUILD_DIR="build"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    -*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      BUILD_DIR="$1"
      shift
      ;;
  esac
done

# Find and source the zephyr environment script, typically via the sof-venv wrapper
# or directly if running in a known zephyrproject layout.
# We will use the existing helper sof-venv.sh to get the right environment.

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOF_WORKSPACE="$(dirname "$(dirname "$SCRIPT_DIR")")"

# Use the SOF workspace to locate the virtual environment
VENV_DIR="$SOF_WORKSPACE/.venv"
echo "Using SOF environment at $SOF_WORKSPACE"

# start the virtual environment
source ${VENV_DIR}/bin/activate

# Execute the QEMU runner from within the correct build directory
cd "${BUILD_DIR}" || exit 1

# Finally run the python script which will now correctly inherit 'west' from the sourced environment.
python3 "${SCRIPT_DIR}/sof-qemu-run.py" --build-dir .

