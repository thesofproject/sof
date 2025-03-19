# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_VOLUME STREQUAL "m")
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/volume_llext)
  add_dependencies(app volume)
else()
  add_local_sources(sof
    volume_generic.c
    volume_hifi3.c
    volume_hifi4.c
    volume_hifi5.c
    volume_generic_with_peakvol.c
    volume_hifi3_with_peakvol.c
    volume_hifi4_with_peakvol.c
    volume_hifi5_with_peakvol.c
    volume.c)
  if(CONFIG_IPC_MAJOR_3)
    add_local_sources(sof volume_ipc3.c)
  elseif(CONFIG_IPC_MAJOR_4)
    add_local_sources(sof volume_ipc4.c)
  endif()
endif()
