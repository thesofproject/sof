# SPDX-License-Identifier: BSD-3-Clause
#
# Cross-build RNNoise from the xiph/rnnoise repository for the webrtc_ns2
# LLEXT module. RNNoise is pure C, requires libm (expf, tanhf, sinf, cosf,
# sqrtf), and carries built-in model weights in rnnoise_tables.c.
#
# Pinned SHA: 70f1d256acd4b34a572f999a05c87bf00b67730d (Feb 2025)
#
# Produces:
#   ${WEBRTC_NS2_INSTALL_DIR}/lib/librnnoise.a
#   ${WEBRTC_NS2_INSTALL_DIR}/include/rnnoise.h

if(NOT DEFINED SOF_RNNOISE_SRC_DIR)
  set(SOF_RNNOISE_SRC_DIR "${sof_top_dir}/../modules/audio/rnnoise"
      CACHE PATH "xiph/rnnoise source directory (west-pinned)")
endif()
cmake_path(NORMAL_PATH SOF_RNNOISE_SRC_DIR)
if(NOT EXISTS "${SOF_RNNOISE_SRC_DIR}/src/denoise.c")
  message(FATAL_ERROR
    "webrtc_ns2: RNNoise source not found at '${SOF_RNNOISE_SRC_DIR}'.\n"
    "Run 'west update' (rnnoise is pinned in west.yml) or pass\n"
    "-DSOF_RNNOISE_SRC_DIR=<path>.")
endif()

get_filename_component(_tc_dir  "${CMAKE_C_COMPILER}" DIRECTORY)
get_filename_component(_tc_name "${CMAKE_C_COMPILER}" NAME)
string(REGEX REPLACE "gcc$" "" _tc_prefix_name "${_tc_name}")
set(_rnn_cross_prefix "${_tc_dir}/${_tc_prefix_name}")

set(WEBRTC_NS2_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/rnnoise-install"
    CACHE INTERNAL "webrtc_ns2: RNNoise library install prefix")

set(_rnn_src "${SOF_RNNOISE_SRC_DIR}/src")
set(_rnn_inc "${SOF_RNNOISE_SRC_DIR}/include")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rnnoise-build.sh"
"#!/bin/sh
set -e
SRC=${_rnn_src}
INC=${_rnn_inc}
INST=${WEBRTC_NS2_INSTALL_DIR}
OBJ=${CMAKE_CURRENT_BINARY_DIR}/rnnoise-obj

mkdir -p \"$OBJ\" \"$INST/lib\" \"$INST/include\"

CFLAGS=\"-O2 -fPIC -I$SRC -I$INC -DOUTSIDE_SPEEX -DRANDOM_PREFIX=rnn\"

# Core RNNoise sources (excludes dump_features.c and train-only files).
SRCS=\"
  $SRC/denoise.c
  $SRC/rnn.c
  $SRC/rnn_data.c
  $SRC/pitch.c
  $SRC/celt_lpc.c
  $SRC/kiss_fft.c
\"

for f in \$SRCS; do
  bn=\$(basename \"\$f\" .c)
  ${CMAKE_C_COMPILER} \$CFLAGS -c \"\$f\" -o \"$OBJ/\${bn}.o\"
done

${_rnn_cross_prefix}ar rcs \"$INST/lib/librnnoise.a\" \"$OBJ\"/*.o
cp \"$INC/rnnoise.h\" \"$INST/include/\"
")

add_custom_command(
  OUTPUT "${WEBRTC_NS2_INSTALL_DIR}/lib/librnnoise.a"
         "${WEBRTC_NS2_INSTALL_DIR}/include/rnnoise.h"
  COMMAND sh "${CMAKE_CURRENT_BINARY_DIR}/rnnoise-build.sh"
  VERBATIM)

add_custom_target(rnnoise_ext
  DEPENDS "${WEBRTC_NS2_INSTALL_DIR}/lib/librnnoise.a")

message(STATUS "webrtc_ns2: cross-building RNNoise from ${SOF_RNNOISE_SRC_DIR}")
