# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_ZEPHYR_NATIVE_DRIVERS)
	zephyr_library_sources(
		host-zephyr.c
	)
else()
	zephyr_library_sources(
		host-legacy.c
	)
endif()