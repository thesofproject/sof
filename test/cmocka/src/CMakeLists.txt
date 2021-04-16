# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(audio)
if(NOT BUILD_UNIT_TESTS_HOST)
	add_subdirectory(debugability)
endif()
add_subdirectory(lib)
add_subdirectory(list)
add_subdirectory(math)
