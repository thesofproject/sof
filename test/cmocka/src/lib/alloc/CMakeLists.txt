# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(alloc
	alloc.c
	${PROJECT_SOURCE_DIR}/src/lib/alloc.c
	${PROJECT_SOURCE_DIR}/src/debug/panic.c
	${PROJECT_SOURCE_DIR}/src/platform/intel/cavs/lib/memory.c
	${PROJECT_SOURCE_DIR}/src/spinlock.c
)

target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/intel/cavs/include)
