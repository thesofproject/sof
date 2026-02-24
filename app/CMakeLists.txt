# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21.0)

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(sample_sof)

target_sources(app PRIVATE
    src/main.c
  )

zephyr_library_include_directories(app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/arch/xtensa/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/include
  )
