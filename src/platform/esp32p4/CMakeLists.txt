# SPDX-License-Identifier: BSD-3-Clause

sof_add_include_directory(include)

add_subdirectory(lib)

sof_add_files(
	platform.c
)
