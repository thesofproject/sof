# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

project(SOF_TOPOLOGY2_DOC NONE)

set(SOF_ROOT_SOURCE_DIRECTORY "${PROJECT_SOURCE_DIR}/..")

set(top_srcdir "${SOF_ROOT_SOURCE_DIRECTORY}")
set(top_bindir "${PROJECT_BINARY_DIR}")

configure_file(
	"${PROJECT_SOURCE_DIR}/sof.doxygen.in"
	"${PROJECT_BINARY_DIR}/sof.doxygen"
)

file(GLOB_RECURSE topology2_sources "${SOF_ROOT_SOURCE_DIRECTORY}/*.conf")
file(GLOB_RECURSE extra_sources "${PROJECT_SOURCE_DIR}/extra-contents/*.doxy")

add_custom_command(
	OUTPUT ${PROJECT_BINARY_DIR}/contents.doxy
	COMMAND ${PROJECT_SOURCE_DIR}/topology2-generate-contents.sh > ${PROJECT_BINARY_DIR}/contents.doxy
	DEPENDS ${PROJECT_SOURCE_DIR}/topology2-generate-contents.sh
	DEPENDS ${topology2_sources}
	VERBATIM
)

add_custom_target(doc-contents
	DEPENDS ${PROJECT_BINARY_DIR}/contents.doxy
)

add_custom_command(
	OUTPUT ${PROJECT_BINARY_DIR}/doxygen/html/index.html
	COMMAND doxygen sof.doxygen
	DEPENDS ${PROJECT_BINARY_DIR}/sof.doxygen
	DEPENDS ${PROJECT_SOURCE_DIR}/topology2-filter.py
	DEPENDS doc-contents
	DEPENDS ${extra_sources}
	DEPENDS ${topology2_sources}
	VERBATIM
	USES_TERMINAL
)

add_custom_target(doc ALL
	DEPENDS ${PROJECT_BINARY_DIR}/doxygen/html/index.html
	VERBATIM
)
