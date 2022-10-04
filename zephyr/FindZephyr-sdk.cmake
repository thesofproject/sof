# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2022, Nordic Semiconductor ASA

# This is copied from the much bigger
# zephyr/cmake/modules/FindZephyr-sdk.cmake.

find_package(Zephyr-sdk ${Zephyr-sdk_FIND_VERSION} REQUIRED QUIET CONFIG PATHS
                 /opt/toolchains # in zephyr-build but not in zephyr.git, go figure
                 /usr
                 /usr/local
                 /opt
                 $ENV{HOME}
                 $ENV{HOME}/.local
                 $ENV{HOME}/.local/opt
                 $ENV{HOME}/bin)

if(Zephyr-sdk_FOUND)
  execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${ZEPHYR_SDK_INSTALL_DIR}")
else()
  message(FATAL_ERROR "Zephyr SDK not found")
endif()
