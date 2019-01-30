include(ExternalProject)

add_library(cmocka STATIC IMPORTED)

if(DEFINED CMOCKA_DIRECTORY)
	# Prebuilt Cmocka lib
	set_property(TARGET cmocka PROPERTY IMPORTED_LOCATION "${CMOCKA_DIRECTORY}/lib/libcmocka-static.a")
	set(CMOCKA_INCLUDE_DIR "${CMOCKA_DIRECTORY}/include")
else()
	# Build Cmocka locally
	ExternalProject_Add(cmocka_git
		GIT_REPOSITORY https://github.com/thesofproject/cmocka
		PREFIX "${PROJECT_BINARY_DIR}/cmocka_git"
		BINARY_DIR "${PROJECT_BINARY_DIR}/cmocka_git/build"
		CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
			-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
			-DWITH_SHARED_LIB=OFF
			-DWITH_STATIC_LIB=ON
			-DWITH_EXAMPLES=OFF
			-DWITH_POSITION_INDEPENDENT_CODE=OFF
			-DWITH_TINY_CONFIG=ON
		BUILD_COMMAND $(MAKE) cmocka-static
		INSTALL_COMMAND ""
	)

	ExternalProject_Get_Property(cmocka_git binary_dir)

	set_property(TARGET cmocka PROPERTY IMPORTED_LOCATION "${binary_dir}/src/libcmocka-static.a")

	add_dependencies(cmocka cmocka_git)

	ExternalProject_Get_Property(cmocka_git source_dir)
	set(CMOCKA_INCLUDE_DIR ${source_dir}/include)
endif()

# linker script, just for log entries
set(memory_mock_lds_in ${PROJECT_SOURCE_DIR}/test/cmocka/memory_mock.x.in)
set(memory_mock_lds_out ${PROJECT_BINARY_DIR}/test/cmocka/memory_mock.x)

add_custom_command(OUTPUT ${memory_mock_lds_out}
	COMMAND cat ${memory_mock_lds_in} | ${CMAKE_C_COMPILER} -E -P - > ${memory_mock_lds_out}
	DEPENDS ${memory_mock_lds_in}
	WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
	COMMENT "Generating linker script: ${memory_mock_lds_out}"
	VERBATIM
	USES_TERMINAL
)

add_custom_target(ld_script_memory_mock DEPENDS ${memory_mock_lds_out})

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
	target_compile_definitions(${test_name} PRIVATE -DCHECK)
	add_test(NAME ${test_name} COMMAND xt-run --exit_with_target_code ${test_name})
endfunction()

add_subdirectory(src)
