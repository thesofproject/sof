# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_PHASE_VOCODER STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/phase_vocoder_llext)
  add_dependencies(app phase_vocoder)
else()
  add_local_sources(sof phase_vocoder.c)
  add_local_sources(sof phase_vocoder_setup.c)
  add_local_sources(sof phase_vocoder_common.c)
  add_local_sources(sof phase_vocoder-generic.c)

  if(CONFIG_IPC_MAJOR_4)
    add_local_sources(sof phase_vocoder-ipc4.c)
  endif()
endif()
