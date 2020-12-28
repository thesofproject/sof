# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(fft
	fft.c
	${PROJECT_SOURCE_DIR}/src/math/fft/fft.c
	${PROJECT_SOURCE_DIR}/src/audio/buffer.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
)
