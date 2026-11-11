# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(dft3
	dft3.c
	${PROJECT_SOURCE_DIR}/src/math/fft/fft_multi.c
	${PROJECT_SOURCE_DIR}/src/math/fft/fft_multi_generic.c
	${PROJECT_SOURCE_DIR}/src/math/fft/fft_multi_hifi3.c
	${PROJECT_SOURCE_DIR}/src/math/fft/fft_32.c
	${PROJECT_SOURCE_DIR}/src/math/fft/fft_common.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/common_mocks.c
)

cmocka_test(fft_multi
	fft_multi.c
	${PROJECT_SOURCE_DIR}/src/math/fft/fft_multi.c
	${PROJECT_SOURCE_DIR}/src/math/fft/fft_multi_generic.c
	${PROJECT_SOURCE_DIR}/src/math/fft/fft_multi_hifi3.c
	${PROJECT_SOURCE_DIR}/src/math/fft/fft_common.c
	${PROJECT_SOURCE_DIR}/src/math/fft/fft_32.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/notifier_mocks.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/common_mocks.c
)
