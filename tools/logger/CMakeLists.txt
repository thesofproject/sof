# SPDX-License-Identifier: BSD-3-Clause

add_executable(sof-logger
	logger.c
	convert.c
	filter.c
	misc.c
)

if(${CMAKE_HOST_WIN32})
	cmake_minimum_required(VERSION 3.20)
	if(DEFINED ENV{MSYS_INSTALL_DIR})
		set(MSYS_INSTALL_DIR $ENV{MSYS_INSTALL_DIR})
		cmake_path(IS_ABSOLUTE MSYS_INSTALL_DIR IS_MSYS_INSTALL_DIR_ABSOLUTE)
		if(NOT IS_MSYS_INSTALL_DIR_ABSOLUTE)
			message(FATAL_ERROR "Please provide absolute path to MSYS2 installation
				setting MSYS_INSTALL_DIR env variable")
		endif()
		cmake_path(APPEND MSYS_INSTALL_DIR "usr" "include" OUTPUT_VARIABLE MSYS_SYSTEM_INCLUDE_PATH)
		target_include_directories(sof-logger PRIVATE "${MSYS_SYSTEM_INCLUDE_PATH}")
	endif()
endif()

target_compile_options(sof-logger PRIVATE
	-Wall -Werror
)

target_include_directories(sof-logger PRIVATE
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}/rimage/src/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}"
)

install(TARGETS sof-logger DESTINATION bin)
