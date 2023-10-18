# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(eq_fir_process
	eq_fir_process.c
)

target_include_directories(eq_fir_process PRIVATE ${PROJECT_SOURCE_DIR}/src/audio)

# make small version of libaudio so we don't have to care
# about unused missing references

add_compile_options(-DUNIT_TEST)

add_library(audio_for_eq_fir STATIC
	${PROJECT_SOURCE_DIR}/src/audio/eq_fir/eq_fir.c
	${PROJECT_SOURCE_DIR}/src/audio/eq_fir/eq_fir_generic.c
	${PROJECT_SOURCE_DIR}/src/audio/eq_fir/eq_fir_hifi2ep.c
	${PROJECT_SOURCE_DIR}/src/audio/eq_fir/eq_fir_hifi3.c
	${PROJECT_SOURCE_DIR}/src/math/fir_generic.c
	${PROJECT_SOURCE_DIR}/src/math/fir_hifi2ep.c
	${PROJECT_SOURCE_DIR}/src/math/fir_hifi3.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module_adapter.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module_adapter_ipc3.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module/generic.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/audio/source_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_source_utils.c
	${PROJECT_SOURCE_DIR}/src/audio/audio_stream.c
	${PROJECT_SOURCE_DIR}/src/audio/component.c
	${PROJECT_SOURCE_DIR}/src/audio/data_blob.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc3/helper.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-common.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-helper.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-graph.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-params.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-schedule.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-stream.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-xrun.c
)
sof_append_relative_path_definitions(audio_for_eq_fir)

target_link_libraries(audio_for_eq_fir PRIVATE sof_options)

target_link_libraries(eq_fir_process PRIVATE audio_for_eq_fir)
