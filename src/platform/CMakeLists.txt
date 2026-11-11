# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_LIBRARY)
	add_subdirectory(library)
endif()

if(CONFIG_PLATFORM_RP2350)
	add_subdirectory(rp2350)
endif()
