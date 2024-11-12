# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(drc_math_test
	drc_math_test.c
	${PROJECT_SOURCE_DIR}/src/audio/drc/drc_math_generic.c
	${PROJECT_SOURCE_DIR}/src/audio/drc/drc_math_hifi3.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
	${PROJECT_SOURCE_DIR}/src/math/exp_fcn.c
	${PROJECT_SOURCE_DIR}/src/math/exp_fcn_hifi.c
)

target_include_directories(drc_math_test PRIVATE ${PROJECT_SOURCE_DIR}/src/audio)
