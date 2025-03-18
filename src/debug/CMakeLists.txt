# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_GDB_DEBUG)
	add_subdirectory(gdb)
endif()

add_subdirectory(tester)

is_zephyr(it_is)
if(it_is) ###  Zephyr ###

add_subdirectory(debug_stream)
add_subdirectory(telemetry)

else() ###  Not Zephyr ###

add_local_sources(sof panic.c)

endif() # Zephyr
