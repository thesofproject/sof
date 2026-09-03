# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

project(SOF_DOC NONE)

set(SOF_ROOT_SOURCE_DIRECTORY "${PROJECT_SOURCE_DIR}/..")

set(top_srcdir "${SOF_ROOT_SOURCE_DIRECTORY}")

configure_file(
	"${PROJECT_SOURCE_DIR}/sof.doxygen.in"
	"${PROJECT_BINARY_DIR}/sof.doxygen"
)

add_custom_target("doc" ALL
	COMMAND doxygen sof.doxygen
	VERBATIM
	USES_TERMINAL
)

add_custom_target("doc-clean"
	COMMAND rm -rf ${PROJECT_BINARY_DIR}/doxygen
	VERBATIM
	USES_TERMINAL
)
