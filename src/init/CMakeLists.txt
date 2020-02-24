# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof init.c)

add_library(ext_manifest STATIC "")

add_local_sources(ext_manifest
	ext_manifest.c)
sof_append_relative_path_definitions(ext_manifest)

target_link_libraries(ext_manifest sof_options)
target_link_libraries(sof_static_libraries INTERFACE ext_manifest)
