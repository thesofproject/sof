# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_DRC_ARM_SIMD)
  set(drc_arch_sources
    ../drc_arm.c
    ../drc_math_arm.c
  )
elseif(CONFIG_DRC_RISCV_SIMD)
  set(drc_arch_sources
    ../drc_riscv.c
    ../drc_math_riscv.c
  )
else()
  set(drc_arch_sources
    ../drc_generic.c
    ../drc_math_generic.c
    ../drc_hifi3.c
    ../drc_hifi4.c
    ../drc_math_hifi3.c
  )
endif()

sof_llext_build("drc"
	SOURCES ../drc.c
		${drc_arch_sources}
	LIB openmodules
)
