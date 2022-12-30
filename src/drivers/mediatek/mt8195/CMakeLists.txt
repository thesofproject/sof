# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof ipc.c timer.c
	interrupt.c afe-memif.c afe-dai.c afe-drv.c
)

if(CONFIG_TEST_SGEN)
add_local_sources(sof afe-sgen.c)
endif()

