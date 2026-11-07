# SPDX-License-Identifier: BSD-3-Clause

if (CONFIG_IPC_MAJOR_4)
	add_subdirectory(ipc4)
endif()

# Common to Zephyr and XTOS
add_local_sources(sof
	ipc-common.c
	ipc-helper.c
)

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###



endif() # Zephyr
