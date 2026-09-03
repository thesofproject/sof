# SPDX-License-Identifier: BSD-3-Clause

if(BUILD_UNIT_TESTS_HOST)
	cmocka_test(alloc
		alloc.c
		${PROJECT_SOURCE_DIR}/src/lib/alloc.c
		${PROJECT_SOURCE_DIR}/src/platform/library/lib/memory.c
	)
else()
	if(CONFIG_CAVS)
		set(MEMORY_FILE ${PROJECT_SOURCE_DIR}/src/platform/intel/cavs/lib/memory.c)
	endif()
	cmocka_test(alloc
		alloc.c
		${PROJECT_SOURCE_DIR}/src/lib/alloc.c
		${PROJECT_SOURCE_DIR}/src/debug/panic.c
		${MEMORY_FILE}
	)
endif()

target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/intel/cavs/include)
