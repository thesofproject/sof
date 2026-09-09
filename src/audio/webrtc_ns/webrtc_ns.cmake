# SPDX-License-Identifier: BSD-3-Clause
#
# Cross-build the WebRTC Noise Suppression (classic 0.3.x) C sources into
# a static library for the webrtc_ns LLEXT module.
#
# webrtc-audio-processing 0.3.x uses a pure-C noise suppressor with no C++
# and no abseil dependency. We compile the relevant source files directly
# using the Xtensa cross-toolchain (same approach as libfvad in webrtc_vad).
#
# Source is pulled via west from the webrtc-apm west module (which contains
# the webrtc-audio-processing 0.3.1 tarball extraction). See west.yml.
#
# Produces:
#   ${WEBRTC_NS_INSTALL_DIR}/lib/libwebrtc_ns.a
#   ${WEBRTC_NS_INSTALL_DIR}/include/noise_suppression.h

if(NOT DEFINED SOF_WEBRTC_APM_SRC_DIR)
  set(SOF_WEBRTC_APM_SRC_DIR "${sof_top_dir}/../modules/audio/webrtc-apm"
      CACHE PATH "webrtc-audio-processing 0.3.x source directory (west-pinned)")
endif()
cmake_path(NORMAL_PATH SOF_WEBRTC_APM_SRC_DIR)
if(NOT EXISTS "${SOF_WEBRTC_APM_SRC_DIR}/webrtc/modules/audio_processing/ns/ns_core.c")
  message(FATAL_ERROR
    "webrtc_ns: WebRTC APM NS source not found at '${SOF_WEBRTC_APM_SRC_DIR}'.\n"
    "Run 'west update' (webrtc-apm is pinned in west.yml) or pass\n"
    "-DSOF_WEBRTC_APM_SRC_DIR=<path>.")
endif()

if(NOT DEFINED CMAKE_AR)
  set(_ns_ar "ar")
else()
  set(_ns_ar "${CMAKE_AR}")
endif()

set(WEBRTC_NS_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/webrtc-ns-install"
    CACHE INTERNAL "webrtc_ns: NS library install prefix")

set(_ns_src "${SOF_WEBRTC_APM_SRC_DIR}/webrtc")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/webrtc-ns-build.sh"
"#!/bin/sh
set -e
SRC=${_ns_src}
INST=${WEBRTC_NS_INSTALL_DIR}
OBJ=${CMAKE_CURRENT_BINARY_DIR}/webrtc-ns-obj

mkdir -p \"$OBJ\" \"$INST/lib\" \"$INST/include\"

CFLAGS=\"-O2 -fPIC -DWEBRTC_ARCH_32_BITS -DWEBRTC_ARCH_LITTLE_ENDIAN -I${SOF_WEBRTC_APM_SRC_DIR} -I$SRC/modules/audio_processing/ns/include\"

SRCS=\"
  $SRC/modules/audio_processing/ns/noise_suppression.c
  $SRC/modules/audio_processing/ns/ns_core.c
  $SRC/common_audio/fft4g.c
\"

for f in \$SRCS; do
  bn=\$(basename \"\$f\" .c)
  ${CMAKE_C_COMPILER} \$CFLAGS -c \"\$f\" -o \"$OBJ/\${bn}.o\"
done

${_ns_ar} rcs \"$INST/lib/libwebrtc_ns.a\" \"$OBJ\"/*.o
cp $SRC/modules/audio_processing/ns/include/noise_suppression.h \"$INST/include/\"
")

add_custom_command(
  OUTPUT "${WEBRTC_NS_INSTALL_DIR}/lib/libwebrtc_ns.a"
         "${WEBRTC_NS_INSTALL_DIR}/include/noise_suppression.h"
  COMMAND sh "${CMAKE_CURRENT_BINARY_DIR}/webrtc-ns-build.sh"
  VERBATIM)

add_custom_target(webrtc_ns_ext
  DEPENDS "${WEBRTC_NS_INSTALL_DIR}/lib/libwebrtc_ns.a")

message(STATUS "webrtc_ns: cross-building WebRTC NS from ${SOF_WEBRTC_APM_SRC_DIR}")
