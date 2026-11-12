# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21.0)

# Board specific overlays (these must be before find_package(Zephyr))
# -------------------------------------------------------------------

# Intel ACE3.x and newer (e.g. ptl, wcl, nvl): if "os_linux_overlay"
# used, also add "ll_userspace_overlay". The board target may be given
# in full in BOARD ("intel_adsp/ace30/ptl") or split into BOARD and
# BOARD_QUALIFIERS ("intel_adsp" + "ace30/ptl"), so match on both.
if(DEFINED EXTRA_CONF_FILE AND
   "${BOARD}/${BOARD_QUALIFIERS}" MATCHES "^intel_adsp/ace[3-9][0-9]/")
  string(FIND "${EXTRA_CONF_FILE}" "os_linux_overlay" _linux_overlay_idx)
  string(FIND "${EXTRA_CONF_FILE}" "ll_userspace_overlay" _ll_overlay_idx)
  if(_linux_overlay_idx GREATER_EQUAL 0 AND _ll_overlay_idx LESS 0)
    list(APPEND EXTRA_CONF_FILE
      "${CMAKE_CURRENT_SOURCE_DIR}/ll_userspace_overlay.conf")
  endif()
endif()

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(sample_sof)

if(NOT ZEPHYR_SOF_MODULE)
  add_subdirectory(../zephyr sof_zephyr)
endif()

target_sources(app PRIVATE
    src/main.c
  )

target_sources_ifdef(CONFIG_USB_DEVICE_STACK_NEXT app PRIVATE
    src/usbd_init.c
  )

zephyr_library_include_directories(app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/include
  )

if(CONFIG_XTENSA)
  zephyr_library_include_directories(app PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/../src/arch/xtensa/include
    )
endif()

if(CONFIG_SOF_FULL_ZEPHYR_APPLICATION AND NOT CONFIG_XTENSA)
  target_include_directories(app PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/../zephyr/include
      ${CMAKE_CURRENT_SOURCE_DIR}/../src/platform/${PLATFORM}/include
      ${PROJECT_BINARY_DIR}/include/generated
    )

  target_link_libraries(app PRIVATE SOF -Wl,--start-group modules_sof sof_kernel -Wl,--end-group)
  if(TARGET uuid_reg_h)
    add_dependencies(app uuid_reg_h modules_sof sof_kernel)
  endif()
endif()

if(COMMAND sof_append_relative_path_definitions)
  sof_append_relative_path_definitions(app)
endif()
