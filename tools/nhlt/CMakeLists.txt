# SPDX-License-Identifier: BSD-3-Clause

set(sof_source_directory "${PROJECT_SOURCE_DIR}/..")

add_executable(sof-nhlt
	nhlt_parse.c
	dmic.c
	ssp.c
)

target_compile_options(sof-nhlt PRIVATE
	-Wall -Werror
)

target_include_directories(sof-nhlt PRIVATE
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}"
)

# TODO: The topology parser should NOT need to include RTOS header. FIX.
target_include_directories(sof-nhlt PRIVATE ${sof_source_directory}/xtos/include)

install(TARGETS sof-nhlt DESTINATION bin)
