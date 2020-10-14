# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(lib)
add_subdirectory(schedule)

add_local_sources(sof
	panic.c
	platform.c
	init.c
)
