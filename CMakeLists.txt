# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.10)

project(SOF_RIMAGE C)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No CMAKE_BUILD_TYPE, defaulting to Debug")
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
endif()

add_executable(rimage
	src/file_simple.c
	src/cse.c
	src/css.c
	src/plat_auth.c
	src/hash.c
	src/pkcs1_5.c
	src/manifest.c
	src/ext_manifest.c
	src/elf.c
	src/rimage.c
	src/adsp_config.c
	tomlc99/toml.c
)

target_compile_options(rimage PRIVATE
	-Wall -Werror -Wl,-EL -Wmissing-prototypes -Wimplicit-fallthrough=3
)

target_link_libraries(rimage PRIVATE "-lcrypto")

target_include_directories(rimage PRIVATE
	src/include/
	tomlc99/
)

install(TARGETS rimage DESTINATION bin)
