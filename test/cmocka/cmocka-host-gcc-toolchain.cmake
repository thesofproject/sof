# SPDX-License-Identifier: BSD-3-Clause

message(STATUS "Preparing Host GCC toolchain")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_ASM_COMPILER_FORCED 1)
set(CMAKE_C_COMPILER_FORCED 1)

set(CMAKE_ASM_COMPILER_ID GNU)
set(CMAKE_C_COMPILER_ID GNU)

set(CMAKE_C_COMPILER gcc)

find_program(CMAKE_LD NAMES "$ld" PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_AR NAMES "$ar" PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_OBJCOPY NAMES "objcopy" PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_OBJDUMP NAMES "objdump" PATHS ENV PATH NO_DEFAULT_PATH)

set(CMAKE_FIND_ROOT_PATH  ".")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Cmocka is written in C99, but for some reason it sets this flag, only on Posix
# We set up it here, because our system is Generic
add_definitions("-std=gnu99")
