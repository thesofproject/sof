# SPDX-License-Identifier: BSD-3-Clause

find_package(Git)
set(RIMAGE_CMAKE "${SOF_ROOT_SOURCE_DIRECTORY}/rimage/CMakeLists.txt")

if(GIT_FOUND AND EXISTS "${SOF_ROOT_SOURCE_DIRECTORY}/.git")

	if(NOT EXISTS "${RIMAGE_CMAKE}")
		message(STATUS "Git submodules update")

		execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --merge --recursive
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
				RESULT_VARIABLE GIT_SUBMOD_RESULT)

		if(NOT GIT_SUBMOD_RESULT EQUAL "0")
			message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
		endif()
	endif()
endif()

# rimage is not optional, see "git grep include.*rimage"
if(NOT EXISTS "${RIMAGE_CMAKE}")
	message(FATAL_ERROR "rimage not found! Please update git submodules and try again.")
endif()
