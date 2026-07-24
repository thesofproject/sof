# SPDX-License-Identifier: BSD-3-Clause
#
# Cross-build libfvad static library for the webrtc_vad module.
#
# libfvad is a standalone pure-C extraction of the WebRTC GMM VAD. It has
# no build system of its own beyond a trivial set of C source files, so we
# compile them directly with ExternalProject using the Zephyr cross-toolchain.
#
# Source is pulled via west from github.com/dpirch/libfvad (see west.yml).
# Produces ${LIBFVAD_INSTALL_DIR}/lib/libfvad.a and
# ${LIBFVAD_INSTALL_DIR}/include/fvad.h for the LLEXT to link.

include(ExternalProject)

# --- 1. Locate libfvad source (west module) ---
if(NOT DEFINED SOF_LIBFVAD_SRC_DIR)
  set(SOF_LIBFVAD_SRC_DIR "${sof_top_dir}/../modules/audio/libfvad"
      CACHE PATH "libfvad source tree (west-pinned)")
endif()
cmake_path(NORMAL_PATH SOF_LIBFVAD_SRC_DIR)
if(NOT EXISTS "${SOF_LIBFVAD_SRC_DIR}/src/fvad.c")
  message(FATAL_ERROR
    "webrtc_vad: libfvad source not found at '${SOF_LIBFVAD_SRC_DIR}'.\n"
    "Run 'west update' (libfvad is pinned in west.yml) or pass "
    "-DSOF_LIBFVAD_SRC_DIR=<path-to-libfvad-checkout>.")
endif()

# --- 2. Derive cross toolchain prefix from Zephyr target compiler ---
# e.g. .../bin/xtensa-intel_ace30_ptl_zephyr-elf-gcc
#   -> prefix .../bin/xtensa-intel_ace30_ptl_zephyr-elf-
get_filename_component(_tc_dir  "${CMAKE_C_COMPILER}" DIRECTORY)
get_filename_component(_tc_name "${CMAKE_C_COMPILER}" NAME)
string(REGEX REPLACE "gcc$" "" _tc_prefix_name "${_tc_name}")
set(_fvad_cross_prefix "${_tc_dir}/${_tc_prefix_name}")

# --- 3. Output directories ---
set(LIBFVAD_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/libfvad-install"
    CACHE INTERNAL "webrtc_vad: libfvad install prefix")

# libfvad has exactly two source files (fvad.c and the internal vad/ sources).
# Rather than invoking its CMakeLists (which would need a CMake toolchain file),
# we compile the objects directly and archive them. This mirrors the libshine
# approach in ffmpeg.cmake.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fvad-build.sh"
"#!/bin/sh
set -e
export PATH=${_tc_dir}:$ENV{PATH}
SRC=${SOF_LIBFVAD_SRC_DIR}
INST=${LIBFVAD_INSTALL_DIR}
OBJ=${CMAKE_CURRENT_BINARY_DIR}/fvad-obj

mkdir -p \"$OBJ\" \"$INST/lib\" \"$INST/include\"

SRCS=\"
  $SRC/src/fvad.c
  $SRC/src/signal_processing/division_operations.c
  $SRC/src/signal_processing/energy.c
  $SRC/src/signal_processing/get_scaling_square.c
  $SRC/src/signal_processing/resample_48khz.c
  $SRC/src/signal_processing/resample_by_2_internal.c
  $SRC/src/signal_processing/resample_fractional.c
  $SRC/src/signal_processing/spl_inl.c
  $SRC/src/vad/vad_core.c
  $SRC/src/vad/vad_filterbank.c
  $SRC/src/vad/vad_gmm.c
  $SRC/src/vad/vad_sp.c
\"

for f in \$SRCS; do
  bn=\$(basename \"\$f\" .c)
  ${CMAKE_C_COMPILER} -O2 -fPIC \
    -I$SRC/include \
    -I$SRC/src \
    -c \"\$f\" -o \"$OBJ/\${bn}.o\"
done

${_fvad_cross_prefix}ar rcs \"$INST/lib/libfvad.a\" \"$OBJ\"/*.o
cp $SRC/include/fvad.h \"$INST/include/fvad.h\"
")

add_custom_command(
  OUTPUT "${LIBFVAD_INSTALL_DIR}/lib/libfvad.a"
         "${LIBFVAD_INSTALL_DIR}/include/fvad.h"
  COMMAND sh "${CMAKE_CURRENT_BINARY_DIR}/fvad-build.sh"
  VERBATIM)

add_custom_target(fvad_ext
  DEPENDS "${LIBFVAD_INSTALL_DIR}/lib/libfvad.a")

message(STATUS "webrtc_vad: cross-building libfvad from ${SOF_LIBFVAD_SRC_DIR}")
