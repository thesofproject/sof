# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof eq_iir.c eq_iir_generic.c)
if(CONFIG_IPC_MAJOR_3)
	add_local_sources(sof eq_iir_ipc3.c)
elseif(CONFIG_IPC_MAJOR_4)
	add_local_sources(sof eq_iir_ipc4.c)
endif()
