# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_LIBRARY)
	add_subdirectory(library)
endif()

if(CONFIG_PLATFORM_ESP32C6)
	add_subdirectory(esp32c6)
endif()
