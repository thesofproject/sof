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
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/ipc/helper-ipc3.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
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
