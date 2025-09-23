# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_COMP_MIXIN_MIXOUT STREQUAL "m" AND DEFINED CONFIG_LLEXT)

  add_subdirectory(llext ${PROJECT_BINARY_DIR}/mixin_mixout_llext)
  add_dependencies(app mixin_mixout)

else()

  add_local_sources(sof mixin_mixout.c mixin_mixout_generic.c mixin_mixout_hifi3.c mixin_mixout_hifi5.c)

endif()
