# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof src_generic.c src_hifi2ep.c src_hifi3.c src_hifi4.c src.c)

if(CONFIG_IPC_MAJOR_3)
	add_local_sources(sof src_ipc3.c)
elseif(CONFIG_IPC_MAJOR_4)
	add_local_sources(sof src_ipc4.c)
endif()
