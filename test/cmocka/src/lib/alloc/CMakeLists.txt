cmocka_test(alloc
	alloc.c
	mock.c
	${PROJECT_SOURCE_DIR}/src/lib/alloc.c
	${PROJECT_SOURCE_DIR}/src/lib/panic.c
	${PROJECT_SOURCE_DIR}/src/platform/intel/cavs/memory.c
)
