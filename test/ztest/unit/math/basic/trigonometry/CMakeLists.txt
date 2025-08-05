cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_math_trigonometry)

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
	-DUNIT_TEST=1
)

target_sources(app PRIVATE
	trig_test.c
	${SOF_ROOT}/src/math/trig.c
	${SOF_ROOT}/src/math/lut_trig.c
)

# Link math library for standard math functions
target_link_libraries(app PRIVATE m)
