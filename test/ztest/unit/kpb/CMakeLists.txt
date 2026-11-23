cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_kpb_request)

set(SOF_ROOT "${PROJECT_SOURCE_DIR}/../../../..")
set(sof_top_dir ${SOF_ROOT})

# Include SOF CMake functions
include(${SOF_ROOT}/scripts/cmake/misc.cmake)
include(${SOF_ROOT}/scripts/cmake/uuid-registry.cmake)

target_include_directories(app PRIVATE
	${SOF_ROOT}/zephyr/include
	${SOF_ROOT}/src/include
	${SOF_ROOT}/src/kernel/platform/posix/include
	${SOF_ROOT}/src/platform/posix/include
	${PROJECT_BINARY_DIR}/include/generated
)

# Define SOF-specific configurations for unit testing
target_compile_definitions(app PRIVATE
	-DCONFIG_ZEPHYR_POSIX=1
	-DCONFIG_COMP_KPB=1
)

target_sources(app PRIVATE
	test_kpb_request_ztest.c
	${SOF_ROOT}/src/kernel/audio/kpb-kernel.c
)

# Add RELATIVE_FILE definitions for SOF trace functionality
sof_append_relative_path_definitions(app)
