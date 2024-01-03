# SPDX-License-Identifier: BSD-3-Clause

# Files common to Zephyr and XTOS
add_local_sources(sof
	dai.c
	handler.c
	helper.c
	logging.c
	notification.c
)


is_zephyr(it_is)
if(it_is) ###  Zephyr ###

zephyr_library_sources_ifdef(CONFIG_AMS
	ams_helpers.c
)


else()  ### Not Zephyr ####


add_local_sources(sof
	ams_helpers.c
)

target_include_directories(sof_options INTERFACE ${RIMAGE_TOP}/src/include)


endif() # Zephyr
