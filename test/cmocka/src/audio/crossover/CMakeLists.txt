# SPDX-License-Identifier: BSD-3-Clause

cmocka_test(crossover_lr4_test
	crossover_lr4_test.c
	${PROJECT_SOURCE_DIR}/src/math/iir_df1.c
	${PROJECT_SOURCE_DIR}/src/math/iir_df1_generic.c
	${PROJECT_SOURCE_DIR}/src/math/iir_df1_hifi3.c
	${PROJECT_SOURCE_DIR}/src/math/iir_df1_hifi4.c
	${PROJECT_SOURCE_DIR}/src/math/iir_df1_hifi5.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
)

target_link_libraries(crossover_lr4_test PRIVATE m)

# crossover_split_test: tests the split_2way / split_3way / split_4way routing
# topology by compiling crossover_generic.c directly.  The per-format processing
# functions in crossover_generic.c reference the source/sink API; those symbols
# are satisfied by the trivial stubs in crossover_test_mocks.c (shareable by any
# test that links crossover_generic.c).

add_compile_options(-DUNIT_TEST)

cmocka_test(crossover_split_test
	crossover_split_test.c
	crossover_test_mocks.c
	${PROJECT_SOURCE_DIR}/src/audio/crossover/crossover_generic.c
	${PROJECT_SOURCE_DIR}/src/math/iir_df1.c
	${PROJECT_SOURCE_DIR}/src/math/iir_df1_generic.c
	${PROJECT_SOURCE_DIR}/src/math/iir_df1_hifi3.c
	${PROJECT_SOURCE_DIR}/src/math/iir_df1_hifi4.c
	${PROJECT_SOURCE_DIR}/src/math/iir_df1_hifi5.c
	${PROJECT_SOURCE_DIR}/src/math/numbers.c
	${PROJECT_SOURCE_DIR}/src/audio/audio_stream.c
)

target_include_directories(crossover_split_test PRIVATE
	${PROJECT_SOURCE_DIR}/src/audio/crossover
)

target_link_libraries(crossover_split_test PRIVATE m)
