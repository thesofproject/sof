# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_PROBE STREQUAL "m")
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/probe_llext)
  add_dependencies(app probe)
elseif(CONFIG_PROBE)
  add_local_sources(sof probe.c)
endif()
