# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_IPC_MAJOR_3)
  set(ipc_suffix ipc3)
elseif(CONFIG_IPC_MAJOR_4)
  set(ipc_suffix ipc4)
endif()

if(CONFIG_SAMPLE_SMART_AMP STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(smart_amp_test_llext ${PROJECT_BINARY_DIR}/smart_amp_test_llext)
  add_dependencies(app smart_amp_test)
elseif(CONFIG_SAMPLE_SMART_AMP)
  add_local_sources(sof smart_amp_test_${ipc_suffix}.c)
endif()

if(CONFIG_SAMPLE_KEYPHRASE)
  add_local_sources(sof detect_test.c)
endif()

if(CONFIG_KWD_NN_SAMPLE_KEYPHRASE)
  add_local_sources(sof kwd_nn_detect_test.c)
endif()
