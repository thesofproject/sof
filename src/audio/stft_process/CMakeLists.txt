# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_STFT_PROCESS STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/stft_process_llext)
  add_dependencies(app stft_process)
else()
  add_local_sources(sof stft_process.c)
  add_local_sources(sof stft_process_setup.c)
  add_local_sources(sof stft_process_common.c)
  add_local_sources(sof stft_process-generic.c)

  if(CONFIG_IPC_MAJOR_3)
    add_local_sources(sof stft_process-ipc3.c)
  elseif(CONFIG_IPC_MAJOR_4)
    add_local_sources(sof stft_process-ipc4.c)
  endif()
endif()
