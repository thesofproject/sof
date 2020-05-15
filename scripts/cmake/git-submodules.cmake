# SPDX-License-Identifier: BSD-3-Clause

find_package(Git)
if(GIT_FOUND AND EXISTS "${SOF_ROOT_SOURCE_DIRECTORY}/.git")
	# Update submodules by default
	option(GIT_SUBMODULE "Check submodules during build" ON)

	if(GIT_SUBMODULE)
		message(STATUS "Git submodule update")

		execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
				RESULT_VARIABLE GIT_SUBMOD_RESULT)

		if(NOT GIT_SUBMOD_RESULT EQUAL "0")
			message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
		endif()
	endif()
endif()

if(NOT EXISTS "${SOF_ROOT_SOURCE_DIRECTORY}/rimage/CMakeLists.txt")
	message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()
