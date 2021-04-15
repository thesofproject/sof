# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(selector_test
	selector_test.c
)

target_include_directories(selector_test PRIVATE ${PROJECT_SOURCE_DIR}/src/audio)

# make small version of libaudio so we don't have to care
# about unused missing references

add_compile_options(-DUNIT_TEST)

add_library(audio_for_selector STATIC
	${PROJECT_SOURCE_DIR}/src/audio/selector/selector.c
	${PROJECT_SOURCE_DIR}/src/audio/selector/selector_generic.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/ipc/helper-ipc3.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-common.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-graph.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-params.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-schedule.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-stream.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-xrun.c
)
sof_append_relative_path_definitions(audio_for_selector)

target_link_libraries(audio_for_selector PRIVATE sof_options)

target_link_libraries(selector_test PRIVATE audio_for_selector)
