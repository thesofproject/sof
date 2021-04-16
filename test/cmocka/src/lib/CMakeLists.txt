# SPDX-License-Identifier: BSD-3-Clause

if(NOT BUILD_UNIT_TESTS_HOST)
	add_subdirectory(alloc)
endif()
add_subdirectory(lib)
add_subdirectory(preproc)
