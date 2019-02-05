cmake_minimum_required(VERSION 3.10)

set(TARBALL_VERSION_FILE_NAME ".tarball-version")
set(TARBALL_VERSION_SOURCE_PATH "${PROJECT_SOURCE_DIR}/${TARBALL_VERSION_FILE_NAME}")

if(EXISTS ${TARBALL_VERSION_SOURCE_PATH})
	file(STRINGS ${TARBALL_VERSION_SOURCE_PATH} lines ENCODING "UTF-8")
	list(GET lines 0 GIT_TAG)
	list(GET lines 1 GIT_LOG_HASH)
	message(STATUS "Found ${TARBALL_VERSION_FILE_NAME}")
	message(STATUS "Version: ${GIT_TAG} / ${GIT_LOG_HASH}")
else()
	execute_process(COMMAND git describe --abbrev=4
		OUTPUT_VARIABLE GIT_TAG
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
	)

	execute_process(COMMAND git log --pretty=format:%h -1
		OUTPUT_VARIABLE GIT_LOG_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
	)
endif()

if(NOT GIT_TAG MATCHES "^v")
	set(GIT_TAG "v0.0-0-g0000")
endif()

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

# TODO
set(SOF_BUILD 0)

function(sof_check_version_h version_h_path)
	string(CONCAT header_content
		"#define SOF_MAJOR ${SOF_MAJOR}\n"
		"#define SOF_MINOR ${SOF_MINOR}\n"
		"#define SOF_MICRO ${SOF_MICRO}\n"
		"#define SOF_TAG \"${SOF_TAG}\"\n"
		"#define SOF_BUILD ${SOF_BUILD}\n"
	)

	if(EXISTS "${version_h_path}")
		file(READ "${version_h_path}" old_version_content)
		if(header_content STREQUAL old_version_content)
			return()
		endif()
	endif()	

	message(STATUS "Generating ${version_h_path}")
	file(WRITE "${version_h_path}" "${header_content}")
endfunction()

function(sof_add_version_h_rule
	version_cmake_path)
	
	add_custom_command(OUTPUT ${VERSION_H_PATH}
		COMMAND ${CMAKE_COMMAND} -DVERSION_H_PATH=${VERSION_H_PATH} -P ${version_cmake_path}
		VERBATIM
		USES_TERMINAL
	)
endfunction()

sof_check_version_h("${VERSION_H_PATH}")
