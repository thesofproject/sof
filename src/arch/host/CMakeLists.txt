# SPDX-License-Identifier: BSD-3-Clause

# includes
target_include_directories(sof_public_headers INTERFACE ${PROJECT_SOURCE_DIR}/src/arch/host/include)
target_include_directories(sof_public_headers INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/library/include)
include(CheckCCompilerFlag)

if (NOT CONFIG_SOF_ZEPHYR_STRICT_HEADERS)
target_include_directories(sof_public_headers INTERFACE
	${PROJECT_SOURCE_DIR}/posix/include
)
endif()

# -Wimplicit-fallthrough is preferred, check if it's supported
check_c_compiler_flag(-Wimplicit-fallthrough supports_implicit_fallthrough)
if (supports_implicit_fallthrough)
    set(implicit_fallthrough -Wimplicit-fallthrough)
endif()

# C & ASM flags
target_compile_options(sof_options INTERFACE -g -O3 -fPIC -DPIC -std=c99 -std=gnu99 -fgnu89-inline
  -Wall -Werror -Wmissing-prototypes ${implicit_fallthrough} -Wno-pointer-to-int-cast
  -Wno-int-to-pointer-cast -Wpointer-arith
  -DCONFIG_LIBRARY "-imacros${CONFIG_H_PATH}")

if(NOT BUILD_UNIT_TESTS_HOST)
	add_subdirectory(lib)
endif()
