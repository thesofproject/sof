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
)
sof_append_relative_path_definitions(audio_mux)

target_link_libraries(audio_mux PRIVATE sof_options)

link_libraries(audio_mux)

cmocka_test(
	mux_get_processing_function
	mux_get_processing_function.c
	mock.c
)

cmocka_test(
	mux_copy
	mux_copy.c
	mock.c
)

cmocka_test(
	demux_copy
	demux_copy.c
	mock.c
)

if(CONFIG_FORMAT_S16LE)
cmocka_test(
	mux_generic_calc_sample_s16le
	mux_generic_calc_sample_s16le.c
	mock.c
)
endif()

if(CONFIG_FORMAT_S24LE)
cmocka_test(
	mux_generic_calc_sample_s24le
	mux_generic_calc_sample_s24le.c
	mock.c
)
endif()


if(CONFIG_FORMAT_S32LE)
cmocka_test(
	mux_generic_calc_sample_s32le
	mux_generic_calc_sample_s32le.c
	mock.c
)
endif()
