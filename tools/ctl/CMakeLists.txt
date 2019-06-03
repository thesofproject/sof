# SPDX-License-Identifier: BSD-3-Clause

add_executable(sof-ctl
	ctl.c
)

target_link_libraries(sof-ctl PRIVATE
	"-lasound"
)

install(TARGETS sof-ctl DESTINATION bin)
