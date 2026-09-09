# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_ALIMITER STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/alimiter_llext)
  add_dependencies(app alimiter)
else()
  add_local_sources(sof alimiter.c)
  if(CONFIG_COMP_ALIMITER_STUB)
    add_local_sources(sof alimiter-stub.c)
  else()
    add_local_sources(sof alimiter-limiter.c)
  endif()
endif()
