# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(comp_set_state
	comp_set_state.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
	${PROJECT_SOURCE_DIR}/src/audio/component.c
	${PROJECT_SOURCE_DIR}/src/audio/data_blob.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc3/helper.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-common.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-helper.c
	${PROJECT_SOURCE_DIR}/src/audio/buffers/comp_buffer.c
	${PROJECT_SOURCE_DIR}/src/audio/buffers/audio_buffer.c
	${PROJECT_SOURCE_DIR}/src/audio/source_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_source_utils.c
	${PROJECT_SOURCE_DIR}/src/audio/audio_stream.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-graph.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-params.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-schedule.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-stream.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-xrun.c
	${PROJECT_SOURCE_DIR}/src/module/audio/source_api.c
	${PROJECT_SOURCE_DIR}/src/module/audio/sink_api.c
)
