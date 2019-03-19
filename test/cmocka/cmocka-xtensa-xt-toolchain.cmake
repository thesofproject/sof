message(STATUS "Preparing Xtensa toolchain")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_ASM_COMPILER_FORCED 1)
set(CMAKE_C_COMPILER_FORCED 1)

set(CMAKE_ASM_COMPILER_ID GNU)
set(CMAKE_C_COMPILER_ID GNU)

set(CROSS_COMPILE "xt-")

set(CMAKE_C_COMPILER 	${CROSS_COMPILE}xcc)

set(CMAKE_LD		${CROSS_COMPILE}ld CACHE STRING "")
set(CMAKE_AR		${CROSS_COMPILE}ar CACHE STRING "")
set(CMAKE_OBJCOPY	${CROSS_COMPILE}objcopy)
set(CMAKE_OBJDUMP	${CROSS_COMPILE}objdump)

set(CMAKE_FIND_ROOT_PATH  ".")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Cmocka is written in C99, but for some reason it sets this flag, only on Posix
# We set up it here, because our system is Generic
add_definitions("-std=gnu99")
