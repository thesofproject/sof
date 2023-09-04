# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(mixer
	mixer_test.c
	../module_adapter_test.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/mixer/mixer.c
	${PROJECT_SOURCE_DIR}/src/audio/mixer/mixer_generic.c
	${PROJECT_SOURCE_DIR}/src/audio/mixer/mixer_hifi3.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc3/helper.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-common.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-helper.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module_adapter.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module_adapter_ipc3.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module/generic.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/audio/source_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_source_utils.c
	${PROJECT_SOURCE_DIR}/src/audio/audio_stream.c
	${PROJECT_SOURCE_DIR}/src/audio/component.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-graph.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-params.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-schedule.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-stream.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-xrun.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
)
target_link_libraries(mixer PRIVATE -lm)
