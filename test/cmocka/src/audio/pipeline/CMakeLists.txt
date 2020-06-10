# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_MULTICORE)
	SET(arch_src ${PROJECT_SOURCE_DIR}/src/drivers/intel/cavs/idc.c)

	# make small lib for stripping so we don't have to care
	# about unused missing references

	add_compile_options(-fdata-sections -ffunction-sections)
	link_libraries(-Wl,--gc-sections)

	add_library(pipeline_lib STATIC ${arch_src})
	sof_append_relative_path_definitions(pipeline_lib)

	target_link_libraries(pipeline_lib PRIVATE sof_options)

	link_libraries(pipeline_lib)
endif()

cmocka_test(pipeline_new
	pipeline_new.c
	pipeline_mocks.c
	pipeline_mocks_rzalloc.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline.c
)

cmocka_test(pipeline_connect_upstream
	pipeline_connect_upstream.c
	pipeline_mocks.c
	pipeline_mocks_rzalloc.c
	pipeline_connection_mocks.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline.c
)

cmocka_test(pipeline_free
	pipeline_free.c
	pipeline_mocks.c
	pipeline_mocks_rzalloc.c
	pipeline_connection_mocks.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline.c
)
