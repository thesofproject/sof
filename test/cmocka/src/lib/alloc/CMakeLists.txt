cmocka_test(alloc
	alloc.c
	mock.c
	${PROJECT_SOURCE_DIR}/src/lib/alloc.c
	${PROJECT_SOURCE_DIR}/src/lib/panic.c
	${PROJECT_SOURCE_DIR}/src/platform/intel/cavs/memory.c
)

target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/intel/cavs/include)
