# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof mux.c mux_generic.c)

if(CONFIG_IPC_MAJOR_3)
	add_local_sources(sof mux_ipc3.c)
elseif(CONFIG_IPC_MAJOR_4)
	add_local_sources(sof mux_ipc4.c)
endif()

