# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(a_law_codec
	a_law_codec.c
	${PROJECT_SOURCE_DIR}/src/math/a_law.c
)

cmocka_test(mu_law_codec
	mu_law_codec.c
	${PROJECT_SOURCE_DIR}/src/math/mu_law.c
)
