cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_audio_crossover)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../..")
set(sof_top_dir ${SOF_ROOT})

include(${SOF_ROOT}/scripts/cmake/misc.cmake)
include(${SOF_ROOT}/scripts/cmake/uuid-registry.cmake)

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	${SOF_ROOT}/src/platform/posix/include
	${SOF_ROOT}/src/audio/crossover
	${PROJECT_BINARY_DIR}/include/generated
)

target_compile_definitions(app PRIVATE
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_LIBRARY=1
	-DUNIT_TEST=1
)

target_sources(app PRIVATE
	crossover_lr4_ztest.c
	crossover_split_ztest.c
	crossover_test_mocks.c
	${SOF_ROOT}/src/audio/crossover/crossover_generic.c
	${SOF_ROOT}/src/math/iir_df1.c
	${SOF_ROOT}/src/math/iir_df1_generic.c
	${SOF_ROOT}/src/math/iir_df1_hifi3.c
	${SOF_ROOT}/src/math/iir_df1_hifi4.c
	${SOF_ROOT}/src/math/iir_df1_hifi5.c
	${SOF_ROOT}/src/math/numbers.c
)

# Only the split functions are exercised, so the format-specific passthrough
# helper in crossover_generic.c is intentionally unused in this test build.
set_source_files_properties(
	${SOF_ROOT}/src/audio/crossover/crossover_generic.c
	PROPERTIES COMPILE_OPTIONS "-Wno-unused-function"
)

# native_sim links libm in the runner; keeping it out of the embedded image
# avoids localizing host libm objects compiled with position-independent code.
sof_append_relative_path_definitions(app)
