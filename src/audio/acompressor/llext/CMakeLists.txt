# SPDX-License-Identifier: BSD-3-Clause

add_llext_module(acompressor)

add_local_sources(acompressor ../acompressor.c)
if(CONFIG_COMP_ACOMPRESSOR_STUB)
  add_local_sources(acompressor ../acompressor-stub.c)
else()
  add_local_sources(acompressor ../acompressor-compress.c)
endif()

llext_compile_options(acompressor PRIVATE ${SOF_LLEXT_FLAGS})
