cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_math_advanced_functions)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../../../..")

# Set SOF top directory for UUID registry generation
set(sof_top_dir ${SOF_ROOT})

# Include SOF CMake utilities for proper SOF source compilation
include(${SOF_ROOT}/scripts/cmake/misc.cmake)
include(${SOF_ROOT}/scripts/cmake/uuid-registry.cmake)

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	${SOF_ROOT}/src/platform/posix/include
	${SOF_ROOT}/test/cmocka/include
	${PROJECT_BINARY_DIR}/include/generated  # For uuid-registry.h
)

# Define SOF-specific configurations for unit testing
target_compile_definitions(app PRIVATE
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_LIBRARY=1
	-DUNIT_TEST=1
)

target_sources(app PRIVATE
	test_scalar_power_ztest.c
	${SOF_ROOT}/src/math/power.c
)

# Apply SOF relative path definitions for proper compilation
sof_append_relative_path_definitions(app)

# Link math library for advanced math functions
target_link_libraries(app PRIVATE m)
