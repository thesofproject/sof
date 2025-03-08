# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof asrc.c asrc_farrow.c asrc_farrow_generic.c
	asrc_farrow_hifi3.c asrc_farrow_hifi5.c)

if(CONFIG_IPC_MAJOR_3)
	add_local_sources(sof asrc_ipc3.c)
elseif(CONFIG_IPC_MAJOR_4)
	add_local_sources(sof asrc_ipc4.c)
endif()
