# SPDX-License-Identifier: BSD-3-Clause

message(STATUS "Preparing Xtensa GCC toolchain using ${TOOLCHAIN}")

set(CROSS_COMPILE "${TOOLCHAIN}-")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_ASM_COMPILER_FORCED 1)
set(CMAKE_C_COMPILER_FORCED 1)

set(CMAKE_ASM_COMPILER_ID GNU)
set(CMAKE_C_COMPILER_ID GNU)

set(CMAKE_C_COMPILER 	${CROSS_COMPILE}gcc)

# required for checking flags
set(CMAKE_REQUIRED_FLAGS "-nostdlib")

find_program(CMAKE_LD NAMES "${CROSS_COMPILE}ld" PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_AR NAMES "${CROSS_COMPILE}ar" PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_OBJCOPY NAMES "${CROSS_COMPILE}objcopy" PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_OBJDUMP NAMES "${CROSS_COMPILE}objdump" PATHS ENV PATH NO_DEFAULT_PATH)

set(CMAKE_FIND_ROOT_PATH  ".")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Cmocka is written in C99, but for some reason it sets this flag, only on Posix
# We set up it here, because our system is Generic
add_definitions("-std=gnu99")
