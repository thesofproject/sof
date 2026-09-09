# SPDX-License-Identifier: BSD-3-Clause

add_llext_module(stereowiden)

add_local_sources(stereowiden ../stereowiden.c)
if(CONFIG_COMP_STEREOWIDEN_STUB)
  add_local_sources(stereowiden ../stereowiden-stub.c)
else()
  add_local_sources(stereowiden ../stereowiden-widen.c)
endif()

llext_compile_options(stereowiden PRIVATE ${SOF_LLEXT_FLAGS})
