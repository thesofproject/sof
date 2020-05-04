# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof numbers.c trig.c decibels.c)

if(BUILD_LIBRARY)
	return()
endif()

add_local_sources(sof trig.c decibels.c)