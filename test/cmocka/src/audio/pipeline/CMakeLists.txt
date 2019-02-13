if(CONFIG_SMP)
	SET(arch_src ${PROJECT_SOURCE_DIR}/src/arch/xtensa/smp/idc.c)

	# make small lib for stripping so we don't have to care
	# about unused missing references

	add_compile_options(-fdata-sections -ffunction-sections)
	link_libraries(-Wl,--gc-sections)

	add_library(pipeline_lib STATIC ${arch_src})

	target_link_libraries(pipeline_lib PRIVATE sof_options)

	link_libraries(pipeline_lib)
endif()

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
