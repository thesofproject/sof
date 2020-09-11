# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_FORMAT_FLOAT)
	cmocka_test(pcm_float_generic
		pcm_float.c
		${PROJECT_SOURCE_DIR}/src/audio/pcm_converter/pcm_converter.c
		${PROJECT_SOURCE_DIR}/src/audio/pcm_converter/pcm_converter_generic.c
		${PROJECT_SOURCE_DIR}/src/audio/buffer.c
		${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	)
	target_include_directories(pcm_float_generic PRIVATE ${PROJECT_SOURCE_DIR}/src/include)
	target_compile_definitions(pcm_float_generic PRIVATE PCM_CONVERTER_GENERIC)
	target_link_libraries(pcm_float_generic PRIVATE sof_options)
endif()
