# SPDX-License-Identifier: BSD-3-Clause

if(BUILD_LIBRARY)
  add_local_sources(sof numbers.c)
  return()
endif()

set(base_files numbers.c)

if(CONFIG_CORDIC_FIXED)
  list(APPEND base_files trig.c)
endif()

if(CONFIG_MATH_LUT_SINE_FIXED)
  list(APPEND base_files lut_trig.c)
endif()

if(CONFIG_SQRT_FIXED)
  list(APPEND base_files sqrt_int16.c)
endif()

if(CONFIG_MATH_EXP)
  list(APPEND base_files exp_fcn.c exp_fcn_hifi.c)
endif()

if(CONFIG_MATH_DECIBELS)
  list(APPEND base_files decibels.c)
endif()

if(CONFIG_NATURAL_LOGARITHM_FIXED)
  list(APPEND base_files log_e.c)
endif()

if(CONFIG_COMMON_LOGARITHM_FIXED)
  list(APPEND base_files log_10.c)
endif()

if(CONFIG_POWER_FIXED)
  list(APPEND base_files power.c)
endif()

if(CONFIG_BINARY_LOGARITHM_FIXED)
  list(APPEND base_files base2log.c)
endif()

if(CONFIG_MATH_FIR STREQUAL "m")
  add_subdirectory(fir_llext ${PROJECT_BINARY_DIR}/aux1_fir_llext)
  add_dependencies(app aux1_fir)
elseif(CONFIG_MATH_FIR)
  list(APPEND base_files fir_generic.c fir_hifi2ep.c fir_hifi3.c fir_hifi5.c)
endif()

if(CONFIG_MATH_FFT)
  add_subdirectory(fft)
endif()

if(CONFIG_MATH_IIR STREQUAL "m")
  add_subdirectory(iir_llext ${PROJECT_BINARY_DIR}/aux1_iir_llext)
  add_dependencies(app aux1_iir)
elseif(CONFIG_MATH_IIR)
  if(CONFIG_MATH_IIR_DF2T)
    list(APPEND base_files iir_df2t_generic.c iir_df2t_hifi3.c iir_df2t.c)
  endif()

  if(CONFIG_MATH_IIR_DF1)
    list(APPEND base_files iir_df1_generic.c iir_df1_hifi3.c iir_df1_hifi4.c iir_df1_hifi5.c iir_df1.c)
  endif()
endif()

if(CONFIG_MATH_WINDOW)
  list(APPEND base_files window.c)
endif()

if(CONFIG_MATH_MATRIX)
  list(APPEND base_files matrix.c)
endif()

if(CONFIG_MATH_AUDITORY)
  add_subdirectory(auditory)
endif()

if(CONFIG_MATH_DCT)
  list(APPEND base_files dct.c)
endif()

is_zephyr(it_is)
if(it_is) ###  Zephyr ###

  zephyr_library_sources(
    ${base_files}
  )

else() ### XTOS ###

  add_local_sources(sof ${base_files})

endif()
