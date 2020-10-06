# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_LIBRARY)
	add_local_sources(sof
		ipc.c
	)
	return()
endif()

add_local_sources(sof
	ipc.c
	handler.c
)

if (CONFIG_TRACE)
	add_local_sources(sof
		dma-copy.c)
endif()

if (CONFIG_HOST_PTABLE)
	add_local_sources(sof
		ipc-host-ptable.c)
endif()

add_library(data_structs STATIC "")

# define compiler version
set_property(TARGET data_structs APPEND
	     PROPERTY COMPILE_DEFINITIONS
	     XCC_TOOLS_VERSION="${XCC_TOOLS_VERSION}")

# and optimization settings
get_optimization_flag(optimization_flag)
set_property(TARGET data_structs APPEND
	     PROPERTY COMPILE_DEFINITIONS
	     CC_OPTIMIZE_FLAGS="-${optimization_flag}")

add_local_sources(data_structs
	user_abi_version.c)

target_link_libraries(data_structs sof_options)
target_link_libraries(sof_static_libraries INTERFACE data_structs)
