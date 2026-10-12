# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_EQ_IIR_ARM_SIMD)
  set(eq_iir_arch_sources ../eq_iir_arm.c)
elseif(CONFIG_EQ_IIR_RISCV_SIMD)
  set(eq_iir_arch_sources ../eq_iir_riscv.c)
else()
  set(eq_iir_arch_sources ../eq_iir_generic.c)
endif()

sof_llext_build("eq_iir"
	SOURCES ../eq_iir.c
		../eq_iir_ipc4.c
		${eq_iir_arch_sources}
	LIB openmodules
)
