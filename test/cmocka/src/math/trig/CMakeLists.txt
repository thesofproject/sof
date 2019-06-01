# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(sin_fixed
	sin_fixed.c
	${PROJECT_SOURCE_DIR}/src/math/trig.c
)
