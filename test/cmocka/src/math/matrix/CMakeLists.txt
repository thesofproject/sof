# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(matrix
	matrix.c
	${PROJECT_SOURCE_DIR}/src/math/matrix.c
	${PROJECT_SOURCE_DIR}/test/cmocka/src/common_mocks.c
)
