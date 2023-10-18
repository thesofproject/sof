# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	dai.c
	handler.c
	helper.c
	logging.c
	notification.c
	ams_helpers.c
)

target_include_directories(sof_options INTERFACE ${RIMAGE_TOP}/src/include)
