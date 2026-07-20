# SPDX-License-Identifier: BSD-3-Clause
#
# Cross-build the WebRTC AECm (echo_control_mobile) fixed-point C sources
# for the webrtc_aec LLEXT module.
#
# Source: webrtc-audio-processing 0.3.x (same west module as webrtc_ns).
# The AECm module lives under webrtc/modules/audio_processing/aecm/ and
# depends on webrtc/common_audio/signal_processing/ (same as NS).
#
# Produces:
#   ${WEBRTC_AECM_INSTALL_DIR}/lib/libwebrtc_aecm.a
#   ${WEBRTC_AECM_INSTALL_DIR}/include/echo_control_mobile.h

if(NOT DEFINED SOF_WEBRTC_APM_SRC_DIR)
  set(SOF_WEBRTC_APM_SRC_DIR "${sof_top_dir}/../modules/audio/webrtc-apm"
      CACHE PATH "webrtc-audio-processing 0.3.x source directory (west-pinned)")
endif()
cmake_path(NORMAL_PATH SOF_WEBRTC_APM_SRC_DIR)
if(NOT EXISTS "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/aecm/aecm_core.c")
  message(FATAL_ERROR
    "webrtc_aec: AECm source not found at '${SOF_WEBRTC_APM_SRC_DIR}'.\n"
    "Run 'west update' or pass -DSOF_WEBRTC_APM_SRC_DIR=<path>.")
endif()

get_filename_component(_tc_dir  "${CMAKE_C_COMPILER}" DIRECTORY)
get_filename_component(_tc_name "${CMAKE_C_COMPILER}" NAME)
string(REGEX REPLACE "gcc$" "" _tc_prefix_name "${_tc_name}")
set(_aecm_cross_prefix "${_tc_dir}/${_tc_prefix_name}")

set(WEBRTC_AECM_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/webrtc-aecm-install"
    CACHE INTERNAL "webrtc_aec: AECm library install prefix")

set(_aecm_src "${SOF_WEBRTC_APM_SRC_DIR}/webrtc")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/webrtc-aecm-build.sh"
"#!/bin/sh
set -e
SRC=${_aecm_src}
INST=${WEBRTC_AECM_INSTALL_DIR}
OBJ=${CMAKE_CURRENT_BINARY_DIR}/webrtc-aecm-obj

mkdir -p \"$OBJ\" \"$INST/lib\" \"$INST/include\"

CFLAGS=\"-O2 -fPIC -I$SRC -I$SRC/common_audio/signal_processing/include\"

# AECm core
SRCS=\"
  $SRC/modules/audio_processing/aecm/aecm_core.c
  $SRC/modules/audio_processing/aecm/echo_control_mobile.c
  $SRC/common_audio/signal_processing/complex_bit_reverse.c
  $SRC/common_audio/signal_processing/complex_fft.c
  $SRC/common_audio/signal_processing/cross_correlation.c
  $SRC/common_audio/signal_processing/division_operations.c
  $SRC/common_audio/signal_processing/downsample_fast.c
  $SRC/common_audio/signal_processing/energy.c
  $SRC/common_audio/signal_processing/get_scaling_square.c
  $SRC/common_audio/signal_processing/min_max_operations.c
  $SRC/common_audio/signal_processing/real_fft.c
  $SRC/common_audio/signal_processing/resample_48khz.c
  $SRC/common_audio/signal_processing/resample_by_2_internal.c
  $SRC/common_audio/signal_processing/resample_fractional.c
  $SRC/common_audio/signal_processing/spl_inl.c
  $SRC/common_audio/signal_processing/sqrt_of_one_minus_x_squared.c
  $SRC/common_audio/signal_processing/vector_scaling_operations.c
\"

for f in \$SRCS; do
  bn=\$(basename \"\$f\" .c)
  ${CMAKE_C_COMPILER} \$CFLAGS -c \"\$f\" -o \"$OBJ/\${bn}.o\"
done

${_aecm_cross_prefix}ar rcs \"$INST/lib/libwebrtc_aecm.a\" \"$OBJ\"/*.o
cp $SRC/modules/audio_processing/aecm/echo_control_mobile.h \"$INST/include/\"
")

add_custom_command(
  OUTPUT "${WEBRTC_AECM_INSTALL_DIR}/lib/libwebrtc_aecm.a"
         "${WEBRTC_AECM_INSTALL_DIR}/include/echo_control_mobile.h"
  COMMAND sh "${CMAKE_CURRENT_BINARY_DIR}/webrtc-aecm-build.sh"
  VERBATIM)

add_custom_target(webrtc_aecm_ext
  DEPENDS "${WEBRTC_AECM_INSTALL_DIR}/lib/libwebrtc_aecm.a")

message(STATUS "webrtc_aec: cross-building AECm from ${SOF_WEBRTC_APM_SRC_DIR}")
