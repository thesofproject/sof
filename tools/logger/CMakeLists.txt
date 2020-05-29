# SPDX-License-Identifier: BSD-3-Clause

add_executable(sof-logger
	logger.c
	convert.c
	filter.c
	misc.c
)

target_compile_options(sof-logger PRIVATE
	-Wall -Werror
)

target_include_directories(sof-logger PRIVATE
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}/rimage/src/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}"
)

install(TARGETS sof-logger DESTINATION bin)
