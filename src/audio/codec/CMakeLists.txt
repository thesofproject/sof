# SPDX-License-Identifier: BSD-3-Clause

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

  if(CONFIG_DTS_CODEC STREQUAL "m" AND DEFINED CONFIG_LLEXT)
    add_subdirectory(dts/llext ${PROJECT_BINARY_DIR}/dts_llext)
    add_dependencies(app dts)

  else(CONFIG_DTS_CODEC)
    zephyr_library_sources(dts/dts.c)
    if (CONFIG_DTS_CODEC_STUB)
      zephyr_library_sources(dts/dts_stub.c)
    else()
      zephyr_library_import(DtsCodec
        ${sof_top_dir}/third_party/lib/libdts-sof-interface-i32.a)
    endif()
  endif()

  return()
endif()
