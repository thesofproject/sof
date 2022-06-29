# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof
	dai.c
	handler.c
	helper.c
	logging.c
)

target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/rimage/src/include)
