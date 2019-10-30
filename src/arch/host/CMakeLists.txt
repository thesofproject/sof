# SPDX-License-Identifier: BSD-3-Clause

# includes
target_include_directories(sof_public_headers INTERFACE ${PROJECT_SOURCE_DIR}/src/arch/host/include)
target_include_directories(sof_public_headers INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/library/include)

# C & ASM flags
target_compile_options(sof_options INTERFACE -g -O3 -Wall -Werror -Wl,-EL -Wmissing-prototypes -Wimplicit-fallthrough=3 -Wpointer-arith)

add_subdirectory(lib)
