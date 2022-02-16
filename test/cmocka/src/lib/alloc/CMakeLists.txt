# SPDX-License-Identifier: BSD-3-Clause

if(BUILD_UNIT_TESTS_HOST)
cmocka_test(alloc
	alloc.c
	${PROJECT_SOURCE_DIR}/src/lib/alloc.c
	${PROJECT_SOURCE_DIR}/src/platform/library/lib/memory.c
	${PROJECT_SOURCE_DIR}/src/spinlock.c
)
else()
cmocka_test(alloc
	alloc.c
	${PROJECT_SOURCE_DIR}/src/lib/alloc.c
	${PROJECT_SOURCE_DIR}/src/debug/panic.c

	if(CONFIG_CAVS)
	${PROJECT_SOURCE_DIR}/src/platform/intel/cavs/lib/memory.c
	elseif(CONFIG_ACE)
	${PROJECT_SOURCE_DIR}/src/platform/intel/ace/lib/memory.c
	endif()

	${PROJECT_SOURCE_DIR}/src/spinlock.c
)
endif()

target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/intel/cavs/include)
