# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.10)

project(SOF_TESTBENCH C)

add_executable(testbench
	testbench.c
	alloc.c
	common_test.c
	file.c
	ipc.c
	schedule.c
	edf_schedule.c
	ll_schedule.c
	panic.c
	topology.c
	trace.c
)

target_compile_definitions(testbench PRIVATE -DBUILD_TESTBENCH=1)

target_include_directories(testbench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_options(testbench PRIVATE -g -O3 -Wall -Werror -Wl,-EL -Wmissing-prototypes -Wimplicit-fallthrough=3)

target_link_libraries(testbench PRIVATE -ldl -lm)

install(TARGETS testbench DESTINATION bin)

set(sof_source_directory "${PROJECT_SOURCE_DIR}/../..")
set(sof_install_directory "${PROJECT_BINARY_DIR}/sof_ep/install")
set(sof_binary_directory "${PROJECT_BINARY_DIR}/sof_ep/build")

include(ExternalProject)

ExternalProject_Add(sof_ep
	DOWNLOAD_COMMAND ""
	SOURCE_DIR "${sof_source_directory}"
	PREFIX "${PROJECT_BINARY_DIR}/sof_ep"
	BINARY_DIR "${sof_binary_directory}"
	CMAKE_ARGS -DBUILD_LIBRARY=ON
		-DCMAKE_INSTALL_PREFIX=${sof_install_directory}
		-DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
	BUILD_ALWAYS 1
	BUILD_BYPRODUCTS "${sof_install_directory}/lib/libsof.so"
)

ExternalProject_Add_Step(
	sof_ep defconfig
	COMMAND ${CMAKE_COMMAND} --build . --target library_defconfig
	DEPENDEES configure
	DEPENDERS build
	WORKING_DIRECTORY "${sof_binary_directory}"
)

add_library(sof_library SHARED IMPORTED)
set_target_properties(sof_library PROPERTIES IMPORTED_LOCATION "${sof_install_directory}/lib/libsof.so")
add_dependencies(sof_library sof_ep)

target_link_libraries(testbench PRIVATE sof_library)
target_include_directories(testbench PRIVATE ${sof_install_directory}/include)

set_target_properties(testbench
	PROPERTIES
	INSTALL_RPATH "${sof_install_directory}/lib"
	INSTALL_RPATH_USE_LINK_PATH TRUE
)
