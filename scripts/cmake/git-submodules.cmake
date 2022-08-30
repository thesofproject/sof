# SPDX-License-Identifier: BSD-3-Clause

find_package(Git)
set(RIMAGE_CMAKE "${SOF_ROOT_SOURCE_DIRECTORY}/rimage/CMakeLists.txt")

if(GIT_FOUND AND EXISTS "${SOF_ROOT_SOURCE_DIRECTORY}/.git")

	if(EXISTS "${RIMAGE_CMAKE}")

		# As incredible as it sounds, some people run neither
		# "git status" nor "git diff" every few minutes and not
		# even when their build fails. There has been reports
		# that they're puzzled when they miss a required
		# submodule update. This is an attempt to draw their
		# attention based on the assumption that they pay more
		# attention to the CMake logs. The warning can be
		# silenced with I_KNOW_SUBMODULES, see below.
		execute_process(COMMAND ${GIT_EXECUTABLE} submodule status --recursive
				WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
				OUTPUT_VARIABLE stdout
				RESULT_VARIABLE status)

		if(NOT "${status}" EQUAL "0")
			# stderr was passed-through
			message(FATAL_ERROR
"git submodule status --recursive failed, returned: ${status}\n${stdout}")
		endif()

		if(NOT "$ENV{I_KNOW_SUBMODULES}" AND
		  ("${stdout}" MATCHES "^[^ ]" OR "${stdout}" MATCHES "\n[^ ]"))
			message(WARNING
"There are submodule changes, check git status and git diff\n${stdout}")
		endif()

	elseif(NOT BUILD_UNIT_TESTS AND
	    NOT CONFIG_LIBRARY)

		message(FATAL_ERROR
"${RIMAGE_CMAKE} not found. You should have used 'git clone --recursive'. \
To fix this existing git clone run:
git submodule update --init --merge --recursive
")
	endif() # rimage/CMakeLists.txt

endif() # .git/
