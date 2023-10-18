# SPDX-License-Identifier: BSD-3-Clause

set(RIMAGE_SUBMODULE "${SOF_ROOT_SOURCE_DIRECTORY}/rimage")
if(EXISTS "${RIMAGE_SUBMODULE}/CMakeLists.txt")
	message(WARNING
		"${RIMAGE_SUBMODULE} is deprecated and ignored"
	)
endif()

find_package(Git)
set(TOMLC99_MAKE "${SOF_ROOT_SOURCE_DIRECTORY}/tools/rimage/tomlc99/Makefile")
if(GIT_FOUND AND EXISTS "${SOF_ROOT_SOURCE_DIRECTORY}/.git")

	if(EXISTS "${TOMLC99_MAKE}")

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
"${TOMLC99_MAKE} not found. You should have used 'git clone --recursive'. \
To fix this existing git clone run:
git submodule update --init --merge tools/rimage/tomlc99
")
	endif() # tomlc99/Makefile

endif() # .git/
