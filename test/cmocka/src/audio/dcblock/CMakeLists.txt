# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(dcblock_math_test
	dcblock_math_test.c
	${PROJECT_SOURCE_DIR}/src/audio/dcblock/dcblock_generic.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
)

target_include_directories(dcblock_math_test PRIVATE ${PROJECT_SOURCE_DIR}/src/audio)
target_include_directories(dcblock_math_test PRIVATE ${PROJECT_SOURCE_DIR}/src/audio/dcblock)

cmocka_test(dcblock_process
	dcblock_process.c
)

target_include_directories(dcblock_process PRIVATE ${PROJECT_SOURCE_DIR}/src/audio)
target_include_directories(dcblock_process PRIVATE ${PROJECT_SOURCE_DIR}/src/audio/dcblock)

# make small version of libaudio so we don't have to care
# about unused missing references

add_compile_options(-DUNIT_TEST)

add_library(audio_for_dcblock STATIC
	${PROJECT_SOURCE_DIR}/src/audio/dcblock/dcblock.c
	${PROJECT_SOURCE_DIR}/src/audio/dcblock/dcblock_generic.c
	${PROJECT_SOURCE_DIR}/src/audio/dcblock/dcblock_ipc3.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module_adapter.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module_adapter_ipc3.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module/generic.c
	${PROJECT_SOURCE_DIR}/src/audio/buffers/comp_buffer.c
	${PROJECT_SOURCE_DIR}/src/audio/buffers/audio_buffer.c
	${PROJECT_SOURCE_DIR}/src/audio/source_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_source_utils.c
	${PROJECT_SOURCE_DIR}/src/audio/audio_stream.c
	${PROJECT_SOURCE_DIR}/src/audio/component.c
	${PROJECT_SOURCE_DIR}/src/audio/data_blob.c
	${PROJECT_SOURCE_DIR}/src/module/audio/source_api.c
	${PROJECT_SOURCE_DIR}/src/module/audio/sink_api.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc3/helper.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-common.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-helper.c
	${PROJECT_SOURCE_DIR}/src/lib/objpool.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-graph.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-params.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-schedule.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-stream.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-xrun.c
)
sof_append_relative_path_definitions(audio_for_dcblock)

target_link_libraries(audio_for_dcblock PRIVATE sof_options)

target_link_libraries(dcblock_process PRIVATE audio_for_dcblock)
