# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(mu_law_codec
	mu_law_codec.c
	${PROJECT_SOURCE_DIR}/src/math/mu_law.c
)
