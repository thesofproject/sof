# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(window
	window.c
	${PROJECT_SOURCE_DIR}/src/math/window.c
	${PROJECT_SOURCE_DIR}/src/math/trig.c
	${PROJECT_SOURCE_DIR}/src/math/log_e.c
	${PROJECT_SOURCE_DIR}/src/math/base2log.c
	${PROJECT_SOURCE_DIR}/src/math/decibels.c
)
