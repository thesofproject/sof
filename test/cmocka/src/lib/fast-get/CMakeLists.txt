# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(fast-get-tests
	fast-get-tests.c
	${PROJECT_SOURCE_DIR}/src/audio/volume/volume.c
	${PROJECT_SOURCE_DIR}/src/audio/volume/volume_ipc3.c
	${PROJECT_SOURCE_DIR}/src/audio/volume/volume_generic.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/audio/module_adapter_test.c
	${PROJECT_SOURCE_DIR}/zephyr/lib/fast-get.c
	${PROJECT_SOURCE_DIR}/src/lib/alloc.c
	${PROJECT_SOURCE_DIR}/src/platform/library/lib/memory.c
	${PROJECT_SOURCE_DIR}/src/audio/component.c
	${PROJECT_SOURCE_DIR}/src/audio/buffers/comp_buffer.c
	${PROJECT_SOURCE_DIR}/src/audio/buffers/audio_buffer.c
	${PROJECT_SOURCE_DIR}/src/audio/source_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_api_helper.c
	${PROJECT_SOURCE_DIR}/src/audio/sink_source_utils.c
	${PROJECT_SOURCE_DIR}/src/audio/audio_stream.c
	${PROJECT_SOURCE_DIR}/src/module/audio/source_api.c
	${PROJECT_SOURCE_DIR}/src/module/audio/sink_api.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc3/helper.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-helper.c
	${PROJECT_SOURCE_DIR}/src/ipc/ipc-common.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module_adapter.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module_adapter_ipc3.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module/generic.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module/memory-common.c
	${PROJECT_SOURCE_DIR}/src/audio/module_adapter/module/memory-heap.c
	${PROJECT_SOURCE_DIR}/src/audio/data_blob.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-graph.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-params.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-schedule.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-stream.c
	${PROJECT_SOURCE_DIR}/src/audio/pipeline/pipeline-xrun.c
)

target_link_libraries(fast-get-tests PRIVATE "-Wl,--wrap=rzalloc,--wrap=rmalloc,--wrap=rfree")
target_include_directories(fast-get-tests PRIVATE ${PROJECT_SOURCE_DIR}/src/audio)