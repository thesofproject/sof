# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof numbers.c trig.c decibels.c iir_df2t_generic.c iir_df2t_hifi3.c)

if(BUILD_LIBRARY)
	return()
endif()

add_local_sources(sof trig.c decibels.c iir_df2t_generic.c iir_df2t_hifi3.c)
