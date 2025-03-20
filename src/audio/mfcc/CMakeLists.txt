# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_MFCC STREQUAL "m")
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/mfcc_llext)
  add_dependencies(app mfcc)
else()
  add_local_sources(sof mfcc.c mfcc_setup.c mfcc_common.c mfcc_generic.c mfcc_hifi4.c mfcc_hifi3.c)
endif()
