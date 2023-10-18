# SPDX-License-Identifier: BSD-3-Clause

# make small lib for stripping so we don't have to care
# about unused missing references

add_compile_options(-fdata-sections -ffunction-sections -DUNIT_TEST)
link_libraries(-Wl,--gc-sections)

add_library(
	audio_mux
	STATIC
	${PROJECT_SOURCE_DIR}/src/audio/mux/mux.c
	${PROJECT_SOURCE_DIR}/src/audio/mux/mux_generic.c
	${PROJECT_SOURCE_DIR}/src/audio/component.c
	${PROJECT_SOURCE_DIR}/src/audio/data_blob.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/audio/source_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_source_utils.c
	${PROJECT_SOURCE_DIR}/src/audio/audio_stream.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc3/helper.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-helper.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module_adapter.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module_adapter_ipc3.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module/generic.c
)
sof_append_relative_path_definitions(audio_mux)

target_link_libraries(audio_mux PRIVATE sof_options)

link_libraries(audio_mux)

cmocka_test(
	mux_get_processing_function
	mux_get_processing_function.c
)

cmocka_test(
	mux_copy
	mux_copy.c
)

cmocka_test(
	demux_copy
	demux_copy.c
)
