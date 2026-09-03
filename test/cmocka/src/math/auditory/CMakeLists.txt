# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(auditory
	auditory.c
	${PROJECT_SOURCE_DIR}/src/math/auditory/auditory.c
	${PROJECT_SOURCE_DIR}/src/math/auditory/mel_filterbank_16.c
	${PROJECT_SOURCE_DIR}/src/math/auditory/mel_filterbank_32.c
	${PROJECT_SOURCE_DIR}/src/math/log_e.c
	${PROJECT_SOURCE_DIR}/src/math/base2log.c
	${PROJECT_SOURCE_DIR}/src/math/decibels.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
)
