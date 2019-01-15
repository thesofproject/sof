cmake_minimum_required(VERSION 3.10)

project(SOF_DOC)

set(SOF_ROOT_SOURCE_DIRECTORY "${PROJECT_SOURCE_DIR}/..")

set(top_srcdir "${SOF_ROOT_SOURCE_DIRECTORY}")

configure_file(
	"${PROJECT_SOURCE_DIR}/sof.doxygen.in"
	"${PROJECT_BINARY_DIR}/sof.doxygen"
)

add_custom_target("doc"
	COMMAND doxygen sof.doxygen
	VERBATIM
	USES_TERMINAL
)

add_custom_target("doc-clean"
	COMMAND rm -rf ${PROJECT_BINARY_DIR}/doxygen
	VERBATIM
	USES_TERMINAL
)
