# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21.0)

# Board specific overlays (these must be before find_package(Zephyr))
# -------------------------------------------------------------------
#
# The default build targets the Linux SOF driver. Overlays added here
# are put before any user given overlays, so that the user overlays
# take precedence.

# The board target may be given in full in BOARD ("intel_adsp/ace30/ptl")
# or split into BOARD and BOARD_QUALIFIERS ("intel_adsp" + "ace30/ptl"),
# so normalize the concatenation of the two.
string(REGEX REPLACE "/+" "/" _sof_board "${BOARD}/${BOARD_QUALIFIERS}")
string(REGEX REPLACE "/$" "" _sof_board "${_sof_board}")
string(REPLACE "/" "_" _sof_board_conf "${_sof_board}")

# Zephyr merges the overlays given in EXTRA_CONF_FILE and the deprecated
# OVERLAY_CONFIG, from both CMake variables and the environment, so look
# at all of them to find out which overlays are used.
set(_sof_conf_files "${EXTRA_CONF_FILE};$ENV{EXTRA_CONF_FILE};${OVERLAY_CONFIG};$ENV{OVERLAY_CONFIG}")

if("${_sof_conf_files}" MATCHES "internal_ci_overlay")
  # Internal CI build: also add the board specific internal CI overlay,
  # if there is one.
  set(_sof_ci_overlay
    "${CMAKE_CURRENT_SOURCE_DIR}/overlays/internal_ci/${_sof_board_conf}.conf")
  if(EXISTS "${_sof_ci_overlay}")
    list(PREPEND EXTRA_CONF_FILE "${_sof_ci_overlay}")
  endif()
elseif("${_sof_board}" MATCHES "^intel_adsp/ace[3-9][0-9]/" AND
       NOT "${_sof_conf_files}" MATCHES "ll_userspace_overlay")
  # Intel ACE3.x and newer (e.g. ptl, wcl, nvl): Linux builds use
  # user-space LL.
  list(PREPEND EXTRA_CONF_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/ll_userspace_overlay.conf")
endif()

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(sample_sof)

target_sources(app PRIVATE
    src/main.c
  )

zephyr_library_include_directories(app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/arch/xtensa/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/include
  )
