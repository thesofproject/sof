# SPDX-License-Identifier: BSD-3-Clause

add_llext_module(equalizer)

add_local_sources(equalizer ../equalizer.c)
if(CONFIG_COMP_EQUALIZER_STUB)
  add_local_sources(equalizer ../equalizer-stub.c)
else()
  add_local_sources(equalizer ../equalizer-filter.c)
endif()

llext_compile_options(equalizer PRIVATE ${SOF_LLEXT_FLAGS})
