# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(dct
	dct.c
	${PROJECT_SOURCE_DIR}/src/math/dct.c
	${PROJECT_SOURCE_DIR}/src/math/trig.c
	${PROJECT_SOURCE_DIR}/src/math/sqrt_int16.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/common_mocks.c
)
