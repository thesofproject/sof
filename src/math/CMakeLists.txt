# SPDX-License-Identifier: BSD-3-Clause

if(BUILD_LIBRARY)
	return()
endif()

add_local_sources(sof numbers.c trig.c decibels.c iir_df2t_generic.c iir_df2t_hifi3.c)

if(CONFIG_MATH_FIR)
        add_local_sources(sof fir_generic.c fir_hifi2ep.c fir_hifi3.c)
endif()

if(CONFIG_MATH_FFT)
	add_subdirectory(fft)
endif()
