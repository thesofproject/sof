# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_FORMAT_FLOAT)
  set(fir_float ../fir_float.c)
else()
  set(fir_float "")
endif()

sof_llext_build("aux1_fir"
	SOURCES fir_common.c
		../fir_generic.c
		../fir_hifi2ep.c
		../fir_hifi3.c
		../fir_hifi5.c
		${fir_float}
	LIB openmodules
)
