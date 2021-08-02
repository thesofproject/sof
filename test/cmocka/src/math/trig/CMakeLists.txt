# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(sin_32b_fixed
	sin_32b_fixed.c
	${PROJECT_SOURCE_DIR}/src/math/trig.c
)

cmocka_test(cos_32b_fixed
	cos_32b_fixed.c
	${PROJECT_SOURCE_DIR}/src/math/trig.c
)

cmocka_test(sin_16b_fixed
	sin_16b_fixed.c
	${PROJECT_SOURCE_DIR}/src/math/trig.c
)

cmocka_test(cos_16b_fixed
	cos_16b_fixed.c
	${PROJECT_SOURCE_DIR}/src/math/trig.c
)

cmocka_test(asin_32b_fixed
	asin_32b_fixed.c
	${PROJECT_SOURCE_DIR}/src/math/trig.c
)

cmocka_test(acos_32b_fixed
	acos_32b_fixed.c
	${PROJECT_SOURCE_DIR}/src/math/trig.c
)

cmocka_test(asin_16b_fixed
	asin_16b_fixed.c
	${PROJECT_SOURCE_DIR}/src/math/trig.c
)

cmocka_test(acos_16b_fixed
	acos_16b_fixed.c
	${PROJECT_SOURCE_DIR}/src/math/trig.c
)
