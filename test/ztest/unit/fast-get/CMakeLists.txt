cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_fast_get)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../..")

# Include SOF CMake functions
include(${SOF_ROOT}/scripts/cmake/misc.cmake)

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	${SOF_ROOT}/src/platform/posix/include
)

# Define SOF-specific configurations for unit testing
target_compile_definitions(app PRIVATE
	-DCONFIG_SOF_LOG_LEVEL=CONFIG_LOG_DEFAULT_LEVEL
	-DCONFIG_ZEPHYR_POSIX=1
)

target_sources(app PRIVATE
	test_fast_get_ztest.c
	${SOF_ROOT}/zephyr/lib/fast-get.c
)

target_link_libraries(app PRIVATE "-Wl,--wrap=rzalloc,--wrap=rmalloc,--wrap=rfree")

# Add RELATIVE_FILE definitions for SOF trace functionality
sof_append_relative_path_definitions(app)
