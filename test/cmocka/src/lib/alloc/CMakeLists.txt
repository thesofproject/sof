# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(alloc
	alloc.c
	${PROJECT_SOURCE_DIR}/src/lib/alloc.c
	${PROJECT_SOURCE_DIR}/src/kernel/platform/library/lib/memory.c
)

target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/kernel/platform/intel/cavs/include)
