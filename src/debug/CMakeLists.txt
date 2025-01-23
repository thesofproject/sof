# SPDX-License-Identifier: BSD-3-Clause

if(CONFIG_GDB_DEBUG)
	add_subdirectory(gdb)
endif()

if(CONFIG_COMP_TESTER)
	add_subdirectory(tester)
endif()

add_local_sources(sof panic.c)
