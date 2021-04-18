# SPDX-License-Identifier: BSD-3-Clause

include(ExternalProject)

add_library(cmocka STATIC IMPORTED)

# Default use XCC unless user asks for GCC
if(BUILD_UNIT_TESTS_HOST)
	set(UT_CC_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/cmocka-host-gcc-toolchain.cmake)
	set(UT_TOOLCHAIN "")
elseif(BUILD_UNIT_TESTS_XTENSA_GCC)
	# GCC xtensa build for future Qemu.
	set(UT_CC_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/cmocka-xtensa-gcc-toolchain.cmake)
	set(UT_TOOLCHAIN "-DTOOLCHAIN=${TOOLCHAIN}")
else()
	# xtensa XCC build for xt-run.
	set(UT_CC_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/cmocka-xtensa-xt-toolchain.cmake)
	set(UT_TOOLCHAIN "")
endif()

if(DEFINED CMOCKA_DIRECTORY)
	# Prebuilt Cmocka lib
	set_property(TARGET cmocka PROPERTY IMPORTED_LOCATION "${CMOCKA_DIRECTORY}/lib/libcmocka-static.a")
	set(CMOCKA_INCLUDE_DIR "${CMOCKA_DIRECTORY}/include")
else()
	set(cmocka_binary_directory "${PROJECT_BINARY_DIR}/cmocka_git/build")

	# Build Cmocka locally
	ExternalProject_Add(cmocka_git
		GIT_REPOSITORY https://github.com/thesofproject/cmocka
		# Don't re-fetch (and change!) the code every time we rebuild
		UPDATE_DISCONNECTED True
		PREFIX "${PROJECT_BINARY_DIR}/cmocka_git"
		BINARY_DIR ${cmocka_binary_directory}
		CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
			-DCMAKE_TOOLCHAIN_FILE=${UT_CC_CONFIG}
			-DWITH_SHARED_LIB=OFF
			-DWITH_STATIC_LIB=ON
			-DWITH_EXAMPLES=OFF
			-DWITH_POSITION_INDEPENDENT_CODE=OFF
			-DWITH_TINY_CONFIG=ON
			${UT_TOOLCHAIN}
		BUILD_BYPRODUCTS "${cmocka_binary_directory}/src/libcmocka-static.a"
		INSTALL_COMMAND ""
	)

	set_property(TARGET cmocka PROPERTY IMPORTED_LOCATION "${cmocka_binary_directory}/src/libcmocka-static.a")

	add_dependencies(cmocka cmocka_git)

	ExternalProject_Get_Property(cmocka_git source_dir)
	set(CMOCKA_INCLUDE_DIR ${source_dir}/include)
endif()

if(CONFIG_CAVS)
	target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/platform/intel/cavs/include)
endif()

# linker script, just for log entries
set(memory_mock_lds_in ${PROJECT_SOURCE_DIR}/test/cmocka/memory_mock.x.in)
set(memory_mock_lds_out ${PROJECT_BINARY_DIR}/test/cmocka/memory_mock.x)

add_custom_command(OUTPUT ${memory_mock_lds_out}
	COMMAND ${CMAKE_C_COMPILER} -E -DLINKER -P -o ${memory_mock_lds_out} -x c ${memory_mock_lds_in}
	DEPENDS ${memory_mock_lds_in}
	WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
	COMMENT "Generating linker script: ${memory_mock_lds_out}"
	VERBATIM
	USES_TERMINAL
)

add_custom_target(ld_script_memory_mock DEPENDS ${memory_mock_lds_out})
SET(arch_src ${PROJECT_SOURCE_DIR}/src/arch/xtensa/idc.c)
add_library(common_mock STATIC ${PROJECT_SOURCE_DIR}/test/cmocka/src/common_mocks.c)
add_dependencies(common_mock cmocka)
target_include_directories(common_mock PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(common_mock PRIVATE sof_options)
target_link_libraries(common_mock PRIVATE m)

# xtensa GCC require some extra options to link
if(BUILD_UNIT_TESTS_XTENSA_GCC)
	target_link_libraries(common_mock PRIVATE gcc)
	target_link_libraries(common_mock PRIVATE c)
	target_link_libraries(common_mock PRIVATE -nostdlib)
endif()
link_libraries(common_mock)
sof_append_relative_path_definitions(common_mock)

# creates exectuable for new test and adds it as test for ctest
function(cmocka_test test_name)
	add_executable(${test_name} "")
	add_local_sources(${test_name} ${ARGN})
	add_dependencies(${test_name} ld_script_memory_mock)
	target_include_directories(${test_name} PRIVATE ${PROJECT_SOURCE_DIR}/test/cmocka/include)
	target_include_directories(${test_name} PRIVATE ${CMOCKA_INCLUDE_DIR})
	target_link_libraries(${test_name} PRIVATE "-T${memory_mock_lds_out}")
	target_link_libraries(${test_name} PRIVATE cmocka)
	target_link_libraries(${test_name} PRIVATE sof_options)
	target_link_libraries(${test_name} PRIVATE common_mock)
	target_compile_definitions(${test_name} PRIVATE -DUNIT_TEST)

	# Cmocka requires this define for stdint.h that defines uintptr
	target_compile_definitions(${test_name} PRIVATE -D_UINTPTR_T_DEFINED)

	add_test(NAME ${test_name} COMMAND xt-run --exit_with_target_code ${test_name})

	sof_append_relative_path_definitions(${test_name})
endfunction()

add_subdirectory(src)
