# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_FIR STREQUAL "m" AND DEFINED CONFIG_LLEXT)

  add_subdirectory(llext ${PROJECT_BINARY_DIR}/eq_fir_llext)
  add_dependencies(app eq_fir)

else()

  add_local_sources(sof eq_fir.c eq_fir_generic.c eq_fir_hifi2ep.c eq_fir_hifi3.c)

  if(CONFIG_IPC_MAJOR_3)
    add_local_sources(sof eq_fir_ipc3.c)
  elseif(CONFIG_IPC_MAJOR_4)
    add_local_sources(sof eq_fir_ipc4.c)
  endif()

endif()
