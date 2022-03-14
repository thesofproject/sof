# SPDX-License-Identifier: BSD-3-Clause
if(CONFIG_CAVS)
add_subdirectory(cavs)
elseif(CONFIG_ACE)
add_subdirectory(ace)
endif()
