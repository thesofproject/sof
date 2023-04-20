# SPDX-License-Identifier: BSD-3-Clause
if(CONFIG_RENOIR)
add_subdirectory(renoir)
elseif(CONFIG_REMBRANDT)
	add_subdirectory(rembrandt)
elseif(CONFIG_PHOENIX)
	add_subdirectory(phoenix)
endif()
