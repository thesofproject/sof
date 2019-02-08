# Implements build counter and adds it as post-build action for sof

cmake_minimum_required(VERSION 3.10)

set(VERSION_BUILD_COUNTER_CMAKE_PATH ${CMAKE_CURRENT_LIST_DIR}/version-build-counter.cmake)

set(BUILD_COUNTER_PATH "${SOF_ROOT_BINARY_DIRECTORY}/.build")

if(NOT EXISTS "${BUILD_COUNTER_PATH}")
	file(WRITE "${BUILD_COUNTER_PATH}" "1")
endif()

file(READ "${BUILD_COUNTER_PATH}" SOF_BUILD)

if(NOT SOF_BUILD MATCHES "^[0-9]+$")
	message(WARNING "Invalid SOF_BUILD - setting to 1")
	set(SOF_BUILD 1)
endif()

function(sof_add_build_counter_rule)
	add_custom_command(
		TARGET sof
		POST_BUILD
		COMMAND ${CMAKE_COMMAND}
			-DSOF_ROOT_BINARY_DIRECTORY=${SOF_ROOT_BINARY_DIRECTORY}
			-DBUILD_COUNTER_INCREMENT=ON
			-P ${VERSION_BUILD_COUNTER_CMAKE_PATH}
		COMMENT "Incrementing build number in ${BUILD_COUNTER_PATH}"
		VERBATIM
		USES_TERMINAL
	)
endfunction()

if(BUILD_COUNTER_INCREMENT)
	MATH(EXPR NEXT_SOF_BUILD "(${SOF_BUILD} + 1) % 65536")
	file(WRITE "${BUILD_COUNTER_PATH}" ${NEXT_SOF_BUILD})
endif()
