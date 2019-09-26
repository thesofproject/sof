# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_SRC)
	add_subdirectory(src)
endif()

if(CONFIG_COMP_VOLUME)
	add_local_sources(sof volume_hifi3.c)
endif()
