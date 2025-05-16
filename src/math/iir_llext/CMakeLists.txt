# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_MATH_IIR_DF1)
  set(df1 ../iir_df1.c ../iir_df1_generic.c ../iir_df1_hifi3.c
	  ../iir_df1_hifi4.c ../iir_df1_hifi5.c)
else()
  set(df1 "")
endif()

if(CONFIG_MATH_IIR_DF2T)
  set(df2t ../iir_df2t.c ../iir_df2t_generic.c ../iir_df2t_hifi3.c)
else()
  set(df2t "")
endif()

if(CONFIG_MATH_IIR_DF1 OR CONFIG_MATH_IIR_DF2T)
  sof_llext_build("aux1_iir"
		  SOURCES iir.c ${df1} ${df2t}
		  LIB openmodules
  )
endif()
