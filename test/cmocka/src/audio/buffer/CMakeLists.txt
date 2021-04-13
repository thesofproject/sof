# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(buffer_copy
	buffer_copy.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/ipc/helper-ipc3.c
)

cmocka_test(buffer_new
	buffer_new.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/ipc/helper-ipc3.c
)

cmocka_test(buffer_wrap
	buffer_wrap.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/ipc/helper-ipc3.c
)

cmocka_test(buffer_write
	buffer_write.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/src/ipc/helper-ipc3.c
)
