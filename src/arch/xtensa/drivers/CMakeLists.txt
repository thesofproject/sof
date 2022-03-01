# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof interrupt.c cache_attr.c)

if(NOT CONFIG_ZEPHYR_SOF_MODULE)
	add_local_sources(sof timer.c)
endif()
