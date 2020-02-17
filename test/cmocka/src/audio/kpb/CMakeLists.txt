# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(kpb
	${PROJECT_SOURCE_DIR}/src/audio/kpb.c
	kpb_buffer.c
	kpb_mock.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/lib/agent.c
	${PROJECT_SOURCE_DIR}/src/spinlock.c
	#${PROJECT_SOURCE_DIR}/src/audio/component.c
)
target_link_libraries(kpb PRIVATE -lm)
