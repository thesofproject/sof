# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof timer.c)

if (CONFIG_SMP)
	add_local_sources(sof idc.c)
endif()
