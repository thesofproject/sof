# SPDX-License-Identifier: BSD-3-Clause

add_executable(sof-ctl
	ctl.c
)

target_link_libraries(sof-ctl PRIVATE
	"-lasound"
)

target_compile_options(sof-ctl PRIVATE
	-Wall -Werror
)

target_include_directories(sof-ctl PRIVATE
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}"
)

install(TARGETS sof-ctl DESTINATION bin)
