# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(tester)

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

add_subdirectory(debug_stream)
add_subdirectory(telemetry)

zephyr_library_sources_ifdef(CONFIG_COLD_STORE_EXECUTE_DEBUG dram.c)
if(CONFIG_SHELL AND (CONFIG_SOC_FAMILY_ESPRESSIF OR CONFIG_PLATFORM_ESP32C6 OR CONFIG_PLATFORM_ESP32P4))
	zephyr_library_sources(shell/esp32_shell.c)
endif()

endif() # Zephyr
