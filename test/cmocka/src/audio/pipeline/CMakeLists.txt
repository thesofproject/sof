cmocka_test(pipeline_new
	pipeline_new.c
	pipeline_mocks.c
	pipeline_mocks_rzalloc.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline.c
)

cmocka_test(pipeline_new_allocation
	pipeline_new_allocation.c
	pipeline_mocks.c
	pipeline_new_allocation_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline.c
)

cmocka_test(pipeline_connect_upstream
	pipeline_connect_upstream.c
	pipeline_mocks.c
	pipeline_mocks_rzalloc.c
	pipeline_connection_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline.c
)

cmocka_test(pipeline_free
	pipeline_free.c
	pipeline_mocks.c
	pipeline_mocks_rzalloc.c
	pipeline_connection_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline.c
)
