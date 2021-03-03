# SPDX-License-Identifier: BSD-3-Clause
if(TOOLCHAIN)
	set(CROSS_COMPILE "${TOOLCHAIN}-")
else()
	message(FATAL_ERROR
		" Please specify toolchain to use.\n"
		" Examples:\n"
		" 	1) cmake -DTOOLCHAIN=xt ...\n"
		" 	2) cmake -DTOOLCHAIN=xtensa-apl-elf ...\n"
	)
endif()

if(BUILD_CLANG_SCAN)
	# scan-build has to set its own compiler,
	# so we need to unset current one
	message(STATUS "Reset C Compiler for scan-build")
	set(CMAKE_C_COMPILER)

	# scan-build proxies only compiler, other tools are used directly
	find_program(CMAKE_AR NAMES "${CROSS_COMPILE}ar" PATHS ENV PATH NO_DEFAULT_PATH)
	find_program(CMAKE_RANLIB NAMES "${CROSS_COMPILE}ranlib" PATHS ENV PATH NO_DEFAULT_PATH)

	set(XCC_TOOLS_VERSION "CLANG-SCAN-BUILD")

	if(TOOLCHAIN STREQUAL "xt")
		set(XCC 1)
	endif()

	return()
endif()

message(STATUS "Preparing Xtensa toolchain")

set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_LIST_DIR}/xtensa-platform.cmake")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_ASM_COMPILER_FORCED 1)
set(CMAKE_C_COMPILER_FORCED 1)

set(CMAKE_ASM_COMPILER_ID GNU)
set(CMAKE_C_COMPILER_ID GNU)

# in case if *_FORCED variables are ignored,
# try to just compile lib instead of executable
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# xt toolchain only partially follows gcc convention
if(TOOLCHAIN STREQUAL "xt")
	set(XCC 1)
	set(CMAKE_C_COMPILER ${CROSS_COMPILE}xcc)
else()
	set(CMAKE_C_COMPILER ${CROSS_COMPILE}gcc)
endif()

find_program(CMAKE_LD NAMES "${CROSS_COMPILE}ld" PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_AR NAMES "${CROSS_COMPILE}ar" PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_RANLIB NAMES "${CROSS_COMPILE}ranlib" PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_OBJCOPY NAMES "${CROSS_COMPILE}objcopy" PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_OBJDUMP NAMES "${CROSS_COMPILE}objdump" PATHS ENV PATH NO_DEFAULT_PATH)

set(CMAKE_FIND_ROOT_PATH  ".")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

if(XCC)
	# get compiler description
	execute_process(
		COMMAND ${CMAKE_C_COMPILER} --show-config=config
		OUTPUT_VARIABLE cc_config_output
		OUTPUT_STRIP_TRAILING_WHITESPACE
		RESULT_VARIABLE show_config_res
	)
	if(NOT ${show_config_res} EQUAL 0)
		message(WARNING "${CMAKE_C_COMPILER} --show-config "
		  "failed with: ${show_config_res}")
	endif()

	string(REGEX MATCH "[a-zA-Z]+-[0-9]+.[0-9]+-[a-zA-Z]*" XCC_TOOLS_VERSION "${cc_config_output}")
	if(NOT XCC_TOOLS_VERSION)
		message(WARNING
		  "Couldn't get ${CMAKE_C_COMPILER} description,"
		  " --show-config printed: '${cc_config_output}'")
		set(XCC_TOOLS_VERSION "UNKNOWN-${CMAKE_SYSTEM_NAME}")
	endif()
else()
	string(REGEX MATCH "([^\/\\]+)$" XCC_TOOLS_VERSION "${TOOLCHAIN}")
endif()

string(LENGTH "${XCC_TOOLS_VERSION}" XCC_TOOLS_VERSION_STR_LEN)
math(EXPR XCC_TOOLS_VERSION_PADDING "(${XCC_TOOLS_VERSION_STR_LEN} + 1) % 4")
if(XCC_TOOLS_VERSION_PADDING EQUAL 0)
	set(XCC_TOOLS_VERSION "<${XCC_TOOLS_VERSION}>")
endif()
