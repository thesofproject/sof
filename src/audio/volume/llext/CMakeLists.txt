# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_VOLUME_ARM_SIMD)
  set(volume_sources
    ../volume_arm.c
    ../volume_arm_with_peakvol.c
  )
elseif(CONFIG_VOLUME_RISCV_SIMD)
  set(volume_sources
    ../volume_riscv.c
    ../volume_riscv_with_peakvol.c
  )
else()
  set(volume_sources
    ../volume_generic.c
    ../volume_hifi3.c
    ../volume_hifi4.c
    ../volume_hifi5.c
    ../volume_generic_with_peakvol.c
    ../volume_hifi3_with_peakvol.c
    ../volume_hifi4_with_peakvol.c
    ../volume_hifi5_with_peakvol.c
  )
endif()

sof_llext_build("volume"
	SOURCES ${volume_sources}
		../volume.c
		../volume_ipc4.c
	LIB openmodules
)
