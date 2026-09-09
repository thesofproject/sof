# SPDX-License-Identifier: BSD-3-Clause

add_llext_module(alimiter)

add_local_sources(alimiter ../alimiter.c)
if(CONFIG_COMP_ALIMITER_STUB)
  add_local_sources(alimiter ../alimiter-stub.c)
else()
  add_local_sources(alimiter ../alimiter-limiter.c)
endif()

llext_compile_options(alimiter PRIVATE ${SOF_LLEXT_FLAGS})
