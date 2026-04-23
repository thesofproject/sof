# SPDX-License-Identifier: BSD-3-Clause

# Files common to Zephyr and XTOS
add_local_sources(sof
	handler-user.c
	handler-kernel.c
	helper.c
	logging.c
	notification.c
	notification-user.c
)

# The DAI interface is not implemented in library builds and
# code depends on Zephyr DAI driver definitions to be available.
if(NOT CONFIG_LIBRARY)
	add_local_sources(sof dai.c)
endif()

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

zephyr_library_sources_ifdef(CONFIG_AMS
	ams_helpers.c
)

else()  ### Not Zephyr ####

target_include_directories(sof_options INTERFACE ${RIMAGE_TOP}/src/include)

endif() # Zephyr
