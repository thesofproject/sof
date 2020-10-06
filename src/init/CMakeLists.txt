# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof init.c)

add_library(ext_manifest STATIC "")

# define compiler version
set_property(TARGET ext_manifest APPEND
	PROPERTY COMPILE_DEFINITIONS
	XCC_TOOLS_VERSION="${XCC_TOOLS_VERSION}")

# and optimization settings
get_optimization_flag(optimization_flag)
set_property(TARGET ext_manifest APPEND
	PROPERTY COMPILE_DEFINITIONS
	CC_OPTIMIZE_FLAGS="-${optimization_flag}")

add_local_sources(ext_manifest
	ext_manifest.c)
sof_append_relative_path_definitions(ext_manifest)

target_link_libraries(ext_manifest sof_options)
target_link_libraries(sof_static_libraries INTERFACE ext_manifest)
