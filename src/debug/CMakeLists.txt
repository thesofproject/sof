# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_GDB_DEBUG)
	add_subdirectory(gdb)
endif()

add_subdirectory(tester)

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

add_subdirectory(debug_stream)
add_subdirectory(telemetry)

zephyr_library_sources_ifdef(CONFIG_COLD_STORE_EXECUTE_DEBUG dram.c)

endif() # Zephyr
