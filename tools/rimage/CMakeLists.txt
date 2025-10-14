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
	src/rimage.c
	src/toml_utils.c
	src/adsp_config.c
	src/misc_utils.c
	src/file_utils.c
	src/elf_file.c
	src/module.c
	tomlc99/toml.c
)

set_property(TARGET rimage PROPERTY C_STANDARD 99)

target_compile_options(rimage PRIVATE
	-Wall -Werror -Wmissing-prototypes -Wimplicit-fallthrough
)

if(CMAKE_C_COMPILER_VERSION VERSION_GREATER 9.1)
	target_compile_options(rimage PRIVATE -Wno-char-subscripts)
endif()

# Windows builds use MSYS2 https://www.msys2.org/ to get linux tools and headers.
# MSYS_INSTALL_DIR variable points to MSYS2 installation directory.
# You may pass it as environmental or cmake configure variable.
if(${CMAKE_HOST_WIN32})
	cmake_minimum_required(VERSION 3.20)
	if(DEFINED ENV{MSYS_INSTALL_DIR} AND NOT MSYS_INSTALL_DIR)
		set(MSYS_INSTALL_DIR $ENV{MSYS_INSTALL_DIR})
	endif()

	if(MSYS_INSTALL_DIR)
		cmake_path(IS_ABSOLUTE MSYS_INSTALL_DIR IS_MSYS_INSTALL_DIR_ABSOLUTE)
		if(NOT IS_MSYS_INSTALL_DIR_ABSOLUTE)
			message(FATAL_ERROR "Please provide absolute path to MSYS2 installation
				setting MSYS_INSTALL_DIR env variable")
		endif()
		# Include standard posix headers. Requires pacman openssl-devel package.
		cmake_path(APPEND MSYS_INSTALL_DIR "usr" "include" OUTPUT_VARIABLE MSYS_SYSTEM_INCLUDE_PATH)
		target_include_directories(rimage PRIVATE "${MSYS_SYSTEM_INCLUDE_PATH}")
	endif()
endif()

target_link_libraries(rimage PRIVATE crypto)

target_include_directories(rimage PRIVATE
	src/include/
	tomlc99/
)
