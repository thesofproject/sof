# SPDX-License-Identifier: BSD-3-Clause

# Generates header for which version is taken from (in order of precedence):
# 	1) .tarball-version file
#	2) git
#
# Version is checked during configuration step and for every target
# that has check_version_h target as dependency

cmake_minimum_required(VERSION 3.10)

set(VERSION_CMAKE_PATH ${CMAKE_CURRENT_LIST_DIR}/version.cmake)


# In an ideal world, every CI engine records the most basic and most
# important information:
# - current date and time
# - git version of the pull request
# - git version of the moving branch it's being merged with
#
# In the real world, some CI results use a random timezone without
# telling which one or don't provide any time at all.
string(TIMESTAMP build_start_time UTC)
message(STATUS "version.cmake starting SOF build at ${build_start_time} UTC")

# Most CI engines test a temporary merge of the pull request with a
# moving target: the latest target branch. In that case the SHA version
# gathered by git describe is disposable hence useless. Only the
# --parents SHA are useful.
message(STATUS "Building git commit with parent(s):")
# Note execute_process() failures are ignored by default (missing git...)
execute_process(
	COMMAND git log --parents --oneline --decorate -n 1 HEAD
	)


# Don't confuse this manual _input_ file with the other, output file of
# the same name auto-generated in the top _build_ directory by "make
# dist", see dist.cmake
set(TARBALL_VERSION_FILE_NAME ".tarball-version")
set(TARBALL_VERSION_SOURCE_PATH "${SOF_ROOT_SOURCE_DIRECTORY}/${TARBALL_VERSION_FILE_NAME}")

if(EXISTS ${TARBALL_VERSION_SOURCE_PATH})
	file(STRINGS ${TARBALL_VERSION_SOURCE_PATH} lines ENCODING "UTF-8")
	list(GET lines 0 GIT_TAG)
	list(GET lines 1 GIT_LOG_HASH)
	message(STATUS "Found ${TARBALL_VERSION_FILE_NAME}")
else()
	# execute_process() errors are not fatal by default!
	execute_process(
	        COMMAND git describe --tags --abbrev=12 --match v* --dirty
		WORKING_DIRECTORY ${SOF_ROOT_SOURCE_DIRECTORY}
		OUTPUT_VARIABLE GIT_TAG
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	execute_process(COMMAND git log --pretty=format:%h -1
		WORKING_DIRECTORY ${SOF_ROOT_SOURCE_DIRECTORY}
		OUTPUT_VARIABLE GIT_LOG_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
endif()

if(NOT GIT_TAG MATCHES "^v")
	message(WARNING
	  "git describe found ${GIT_TAG} / nothing starting with 'v'. Shallow clone?")
	set(GIT_TAG "v0.0-0-g0000")
endif()

message(STATUS "GIT_TAG / GIT_LOG_HASH : ${GIT_TAG} / ${GIT_LOG_HASH}")

string(REGEX MATCH "^v([0-9]+)[.]([0-9]+)([.]([0-9]+))?" ignored "${GIT_TAG}")
set(SOF_MAJOR ${CMAKE_MATCH_1})
set(SOF_MINOR ${CMAKE_MATCH_2})
set(SOF_MICRO ${CMAKE_MATCH_4})

if(NOT SOF_MICRO MATCHES "^[0-9]+$")
	set(SOF_MICRO 0)
endif()

string(SUBSTRING "${GIT_LOG_HASH}" 0 5 SOF_TAG)
if(NOT SOF_TAG)
	set(SOF_TAG 0)
endif()

# Calculate source hash value, used to check ldc file and firmware compatibility
if(EXISTS ${SOF_ROOT_SOURCE_DIRECTORY}/.git/)
	set(SOURCE_HASH_DIR "${SOF_ROOT_BINARY_DIRECTORY}/source_hash")
	file(MAKE_DIRECTORY ${SOURCE_HASH_DIR})
	# list tracked files from src directory
	execute_process(COMMAND git ls-files src
			WORKING_DIRECTORY ${SOF_ROOT_SOURCE_DIRECTORY}
			OUTPUT_FILE "${SOURCE_HASH_DIR}/tracked_file_list"
		)
	# calculate hash of each listed files (from file version saved in file system)
	execute_process(COMMAND git hash-object --stdin-paths
			WORKING_DIRECTORY ${SOF_ROOT_SOURCE_DIRECTORY}
			INPUT_FILE "${SOURCE_HASH_DIR}/tracked_file_list"
			OUTPUT_FILE "${SOURCE_HASH_DIR}/tracked_file_hash_list"
		)
	# then calculate single hash of previously calculated hash list
	execute_process(COMMAND git hash-object --stdin
			WORKING_DIRECTORY ${SOF_ROOT_SOURCE_DIRECTORY}
			OUTPUT_STRIP_TRAILING_WHITESPACE
			INPUT_FILE "${SOURCE_HASH_DIR}/tracked_file_hash_list"
			OUTPUT_VARIABLE SOF_SRC_HASH_LONG
		)
	string(SUBSTRING ${SOF_SRC_HASH_LONG} 0 8 SOF_SRC_HASH)
	message(STATUS "Source content hash: ${SOF_SRC_HASH}")
else()
	if("${GIT_LOG_HASH}")
		string(SUBSTRING "${GIT_LOG_HASH}" 0 8 SOF_SRC_HASH)
	else()
		set(SOF_SRC_HASH "0")
	endif()
	message(WARNING "Source content hash not computed without git, using GIT_LOG_HASH instead")
endif()

# for SOF_BUILD
include(${CMAKE_CURRENT_LIST_DIR}/version-build-counter.cmake)

function(sof_check_version_h)
	string(CONCAT header_content
		"#define SOF_MAJOR ${SOF_MAJOR}\n"
		"#define SOF_MINOR ${SOF_MINOR}\n"
		"#define SOF_MICRO ${SOF_MICRO}\n"
		"#define SOF_TAG \"${SOF_TAG}\"\n"
		"#define SOF_BUILD ${SOF_BUILD}\n"
		"#define SOF_GIT_TAG \"${GIT_TAG}\"\n"
		"#define SOF_SRC_HASH 0x${SOF_SRC_HASH}\n"
	)

	if(EXISTS "${VERSION_H_PATH}")
		file(READ "${VERSION_H_PATH}" old_version_content)
		if("${header_content}" STREQUAL "${old_version_content}")
			message(STATUS "Up-to-date ${VERSION_H_PATH}")
			return()
		endif()
	endif()	

	message(STATUS "Generating ${VERSION_H_PATH}")
	file(WRITE "${VERSION_H_PATH}" "${header_content}")
endfunction()

# Run these only if not run as script
if("${CMAKE_SCRIPT_MODE_FILE}" STREQUAL "")
	add_custom_target(
		check_version_h
		BYPRODUCTS ${VERSION_H_PATH}
		COMMAND ${CMAKE_COMMAND}
			-DVERSION_H_PATH=${VERSION_H_PATH}
			-DSOF_ROOT_SOURCE_DIRECTORY=${SOF_ROOT_SOURCE_DIRECTORY}
			-DSOF_ROOT_BINARY_DIRECTORY=${SOF_ROOT_BINARY_DIRECTORY}
			-P ${VERSION_CMAKE_PATH}
		COMMENT "Checking ${VERSION_H_PATH}"
		VERBATIM
		USES_TERMINAL
	)
endif()

sof_check_version_h()
