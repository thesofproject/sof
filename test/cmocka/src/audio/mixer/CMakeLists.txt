# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(mixer
	mixer_test.c
	comp_mock.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/audio/mixer/mixer.c
	${PROJECT_SOURCE_DIR}/src/audio/mixer/mixer_generic.c
	${PROJECT_SOURCE_DIR}/src/audio/mixer/mixer_hifi3.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc3/helper.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-common.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-helper.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-graph.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-params.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-schedule.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-stream.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-xrun.c
)
target_link_libraries(mixer PRIVATE -lm)
