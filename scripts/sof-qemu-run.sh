#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOF_WORKSPACE="$(dirname "$(dirname "$SCRIPT_DIR")")"

TARGET="native_sim"
VALGRIND_ARG=""

usage() {
    cat <<EOF
Usage: $0 [OPTIONS] [TARGET]

Options:
  -h, --help    Show this help message and exit
  --valgrind    Run under valgrind

TARGET          The QEMU target name (e.g., native_sim, qemu_xtensa, qemu_xtensa_mmu).
                The build directory will be resolved to 'build-<TARGET>' relative
                to the workspace. (default: native_sim)
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
  case $1 in
    -h|--help)
      usage
      ;;
    --valgrind)
      VALGRIND_ARG="--valgrind"
      ;;
    *)
      # Allow users who pass the directory name directly out of habit to still work
      if [[ "$1" == build-* ]]; then
        TARGET="${1#build-}"
      else
        TARGET="$1"
      fi
      ;;
  esac
  shift
done

BUILD_DIR="${SOF_WORKSPACE}/build-${TARGET}"

# Find and source the zephyr environment script, typically via the sof-venv wrapper
# or directly if running in a known zephyrproject layout.
# We will use the existing helper sof-venv.sh to get the right environment.

# Use the SOF workspace to locate the virtual environment
VENV_DIR="$SOF_WORKSPACE/.venv"
echo "Using SOF environment at $SOF_WORKSPACE"

# start the virtual environment
source ${VENV_DIR}/bin/activate

# Finally run the python script which will now correctly inherit 'west' from the sourced environment.
python3 "${SCRIPT_DIR}/sof-qemu-run.py" --build-dir "${BUILD_DIR}" $VALGRIND_ARG
