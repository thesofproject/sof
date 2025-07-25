cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_math_arithmetic)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../../..")

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
)

# Define SOF-specific configurations for unit testing
target_compile_definitions(app PRIVATE
	-DCONFIG_SOF_LOG_LEVEL=CONFIG_LOG_DEFAULT_LEVEL
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_LIBRARY=1
	-DCONFIG_NUMBERS_VECTOR_FIND=1
	-DCONFIG_NUMBERS_NORM=1
	-DUNIT_TEST=1
)

target_sources(app PRIVATE
	test_gcd_ztest.c
	test_ceil_divide_ztest.c
	test_find_equal_int16_ztest.c
	test_find_min_int16_ztest.c
	test_find_max_abs_int32_ztest.c
	test_norm_int32_ztest.c
	${SOF_ROOT}/src/math/numbers.c
)

# Link math library for ceil_divide test
target_link_libraries(app PRIVATE m)
