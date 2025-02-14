# SPDX-License-Identifier: BSD-3-Clause

if(BUILD_LIBRARY)
	add_local_sources(sof numbers.c)
	return()
endif()

add_local_sources(sof numbers.c)

if(CONFIG_CORDIC_FIXED)
        add_local_sources(sof trig.c)
endif()

add_local_sources_ifdef(CONFIG_MATH_LUT_SINE_FIXED sof lut_trig.c)

add_local_sources_ifdef(CONFIG_SQRT_FIXED sof sqrt_int16.c)

add_local_sources_ifdef(CONFIG_MATH_EXP sof exp_fcn.c exp_fcn_hifi.c)

if(CONFIG_MATH_DECIBELS)
        add_local_sources(sof decibels.c)
endif()

add_local_sources_ifdef(CONFIG_NATURAL_LOGARITHM_FIXED sof log_e.c)

add_local_sources_ifdef(CONFIG_COMMON_LOGARITHM_FIXED sof log_10.c)

add_local_sources_ifdef(CONFIG_POWER_FIXED sof power.c)

add_local_sources_ifdef(CONFIG_BINARY_LOGARITHM_FIXED sof base2log.c)

add_local_sources_ifdef(CONFIG_MATH_FIR sof fir_generic.c fir_hifi2ep.c fir_hifi3.c fir_hifi5.c)

if(CONFIG_MATH_FFT)
	add_subdirectory(fft)
endif()

add_local_sources_ifdef(CONFIG_MATH_IIR_DF2T sof
	iir_df2t_generic.c iir_df2t_hifi3.c iir_df2t.c)

add_local_sources_ifdef(CONFIG_MATH_IIR_DF1 sof
	iir_df1_generic.c iir_df1_hifi3.c iir_df1_hifi4.c iir_df1_hifi5.c iir_df1.c)

if(CONFIG_MATH_WINDOW)
	 add_local_sources(sof window.c)
endif()

if(CONFIG_MATH_MATRIX)
	 add_local_sources(sof matrix.c)
endif()

if(CONFIG_MATH_AUDITORY)
	 add_subdirectory(auditory)
endif()

if(CONFIG_MATH_DCT)
	 add_local_sources(sof dct.c)
endif()
