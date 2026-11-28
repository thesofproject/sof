# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(component)
add_subdirectory(pipeline)
if(CONFIG_COMP_DCBLOCK)
	add_subdirectory(dcblock)
endif()
