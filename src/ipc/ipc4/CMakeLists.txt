# SPDX-License-Identifier: BSD-3-Clause

# Files common to Zephyr and XTOS
add_local_sources(sof
	dai.c
	handler.c
	helper.c
	logging.c
	notification.c
)

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

zephyr_library_sources_ifdef(CONFIG_AMS
	ams_helpers.c
)

else()  ### Not Zephyr ####

target_include_directories(sof_options INTERFACE ${RIMAGE_TOP}/src/include)

endif() # Zephyr
