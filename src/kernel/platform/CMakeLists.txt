# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_LIBRARY)
	add_subdirectory(library)
elseif(CONFIG_PLATFORM_TEENSY41)
	add_subdirectory(teensy41)
endif()
