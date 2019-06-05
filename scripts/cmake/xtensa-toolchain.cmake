# SPDX-License-Identifier: BSD-3-Clause

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

# xt toolchain only partially follows gcc convention
if(TOOLCHAIN STREQUAL "xt")
	set(XCC 1)
	set(CMAKE_C_COMPILER ${CROSS_COMPILE}xcc)
else()
	set(CMAKE_C_COMPILER ${CROSS_COMPILE}gcc)
endif()

find_program(CMAKE_LD NAMES "${CROSS_COMPILE}ld" PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_AR NAMES "${CROSS_COMPILE}ar" PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_OBJCOPY NAMES "${CROSS_COMPILE}objcopy" PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_OBJDUMP NAMES "${CROSS_COMPILE}objdump" PATHS ENV PATH NO_DEFAULT_PATH)

set(CMAKE_FIND_ROOT_PATH  ".")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
