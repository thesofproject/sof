# SPDX-License-Identifier: BSD-3-Clause
add_subdirectory(common)
if(CONFIG_RENOIR)
	add_subdirectory(renoir)
elseif(CONFIG_REMBRANDT OR CONFIG_ACP_6_3 OR CONFIG_ACP_7_0)
	add_subdirectory(rembrandt)
elseif(CONFIG_VANGOGH)
	add_subdirectory(vangogh)
endif()
