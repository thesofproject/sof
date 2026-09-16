# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21.0)

# Board specific overlays (these must be before find_package(Zephyr))
# -------------------------------------------------------------------

# Intel PTL: if "os_linux_overlay" used, also add "ll_userspace_overlay"
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

add_subdirectory(../zephyr sof_zephyr)

target_sources(app PRIVATE
    src/main.c
    ../src/debug/shell/esp32_shell.c
  )

target_sources_ifdef(CONFIG_USB_DEVICE_STACK_NEXT app PRIVATE
    src/usbd_init.c
  )

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../zephyr/include
    ${PROJECT_BINARY_DIR}/include/generated
  )

if(CONFIG_PLATFORM_ESP32P4)
  target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/platform/esp32p4/include)
elseif(CONFIG_PLATFORM_ESP32C6)
  target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/platform/esp32c6/include)
elseif(CONFIG_PLATFORM_ESP32S3)
  target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/platform/esp32s3/include)
endif()

target_link_libraries(app PRIVATE SOF modules_sof)
target_link_libraries(modules_sof PRIVATE kernel)
if(TARGET uuid_reg_h)
  add_dependencies(app uuid_reg_h modules_sof)
endif()

if(CONFIG_XTENSA)
  target_include_directories(app PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/../src/arch/xtensa/include
    )
endif()

function(sof_append_relative_path_definitions target)
	get_target_property(sources ${target} SOURCES)
	foreach(src ${sources})
		get_filename_component(ABS_PATH ${src} ABSOLUTE)
		file(RELATIVE_PATH rel ${PROJECT_SOURCE_DIR} ${ABS_PATH})
		set_property(
			SOURCE ${src}
			APPEND
			PROPERTY COMPILE_DEFINITIONS
			RELATIVE_FILE="${rel}")
	endforeach()
endfunction()

sof_append_relative_path_definitions(app)


