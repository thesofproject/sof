# SPDX-License-Identifier: BSD-3-Clause

# includes
target_include_directories(sof_public_headers INTERFACE ${PROJECT_SOURCE_DIR}/src/arch/host/include)
target_include_directories(sof_public_headers INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/library/include)

# C & ASM flags
target_compile_options(sof_options INTERFACE -g -O3 -Wall -Werror -Wmissing-prototypes
  -Wimplicit-fallthrough -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wpointer-arith -DCONFIG_LIBRARY "-imacros${CONFIG_H_PATH}")

if(NOT BUILD_UNIT_TESTS_HOST)
	add_subdirectory(lib)
endif()
