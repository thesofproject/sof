# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(scalar_power
	scalar_power.c
	${PROJECT_SOURCE_DIR}/src/math/power.c
)

cmocka_test(base2_logarithm
	base2_logarithm.c
	${PROJECT_SOURCE_DIR}/src/math/base2log.c
)
