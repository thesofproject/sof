# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(scalar_power
	scalar_power.c
	${PROJECT_SOURCE_DIR}/src/math/power.c
)

cmocka_test(base2_logarithm
	base2_logarithm.c
	${PROJECT_SOURCE_DIR}/src/math/base2log.c
)

cmocka_test(square_root
	square_root.c
	${PROJECT_SOURCE_DIR}/src/math/sqrt_int16.c
)

cmocka_test(base_10_logarithm
	base_10_logarithm.c
	${PROJECT_SOURCE_DIR}/src/math/log_10.c
	${PROJECT_SOURCE_DIR}/src/math/base2log.c
)

cmocka_test(base_e_logarithm
	base_e_logarithm.c
	${PROJECT_SOURCE_DIR}/src/math/log_e.c
	${PROJECT_SOURCE_DIR}/src/math/base2log.c
)
