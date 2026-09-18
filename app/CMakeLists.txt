# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21.0)

# Board specific overlays (these must be before find_package(Zephyr)
# ------------------------------------------------------------------

# Intel PTL: use ll_userspace_overlay if building for Linux upstream
if(DEFINED EXTRA_CONF_FILE AND (BOARD MATCHES "/ptl" OR BOARD_QUALIFIERS MATCHES "/ptl"))
  string(FIND "${EXTRA_CONF_FILE}" "os_linux_overlay" _linux_overlay_idx)
  string(FIND "${EXTRA_CONF_FILE}" "ll_userspace_overlay" _ll_overlay_idx)
  if(_linux_overlay_idx GREATER_EQUAL 0 AND _ll_overlay_idx LESS 0)
    list(APPEND EXTRA_CONF_FILE
      "${CMAKE_CURRENT_SOURCE_DIR}/overlays/ptl/ll_userspace_overlay.conf")
  endif()
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
