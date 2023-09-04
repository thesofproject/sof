# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(volume_process
	volume_process.c ../module_adapter_test.c
)

target_include_directories(volume_process PRIVATE ${PROJECT_SOURCE_DIR}/src/audio)

# make small version of libaudio so we don't have to care
# about unused missing references

add_compile_options(-DUNIT_TEST)

add_library(audio_for_volume STATIC
	${PROJECT_SOURCE_DIR}/src/audio/volume/volume.c
	${PROJECT_SOURCE_DIR}/src/audio/volume/volume_ipc3.c
	${PROJECT_SOURCE_DIR}/src/audio/volume/volume_generic.c
	${PROJECT_SOURCE_DIR}/src/audio/volume/volume_hifi3.c
	${PROJECT_SOURCE_DIR}/src/audio/volume/volume_hifi4.c
	${PROJECT_SOURCE_DIR}/src/audio/volume/volume_generic_with_peakvol.c
	${PROJECT_SOURCE_DIR}/src/audio/volume/volume_hifi3_with_peakvol.c
	${PROJECT_SOURCE_DIR}/src/audio/volume/volume_hifi4_with_peakvol.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module_adapter.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module_adapter_ipc3.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module/generic.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/audio/source_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_source_utils.c
	${PROJECT_SOURCE_DIR}/src/audio/audio_stream.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc3/helper.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-common.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-helper.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-graph.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-params.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-schedule.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-stream.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-xrun.c
	${PROJECT_SOURCE_DIR}/src/audio/component.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
)
sof_append_relative_path_definitions(audio_for_volume)

target_link_libraries(audio_for_volume PRIVATE sof_options)

target_link_libraries(volume_process PRIVATE audio_for_volume)
