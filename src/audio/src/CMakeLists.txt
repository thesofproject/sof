# SPDX-License-Identifier: BSD-3-Clause

set(base_files src_generic.c src_hifi2ep.c src_hifi3.c src_hifi4.c src_hifi5.c src_common.c src.c)

if(CONFIG_IPC_MAJOR_3)
  list(APPEND base_files src_ipc3.c)
elseif(CONFIG_IPC_MAJOR_4)
  list(APPEND base_files src_ipc4.c)
endif()

sof_list_append_ifdef(CONFIG_COMP_SRC_LITE base_files src_lite.c)

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

  if(CONFIG_COMP_SRC STREQUAL "m" AND DEFINED CONFIG_LLEXT)

    add_subdirectory(llext ${PROJECT_BINARY_DIR}/src_llext)
    add_dependencies(app src)

  else()

    zephyr_library_sources(${base_files})

  endif()

else() ### library, e.g. testbench or plugin ###

  add_local_sources(sof ${base_files})

endif()
