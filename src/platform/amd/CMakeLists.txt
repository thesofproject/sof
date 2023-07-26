# SPDX-License-Identifier: BSD-3-Clause
add_subdirectory(common)
if(CONFIG_RENOIR)
	add_subdirectory(renoir)
elseif(CONFIG_REMBRANDT)
	add_subdirectory(rembrandt)
elseif(CONFIG_VANGOGH)
	add_subdirectory(vangogh)
endif()
