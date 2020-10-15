# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(platform)
add_subdirectory(arch)
add_subdirectory(ipc)
add_subdirectory(audio)
add_subdirectory(lib)
add_subdirectory(math)
if(CONFIG_SAMPLES)
	add_subdirectory(samples)
endif()
add_local_sources(sof spinlock.c)
add_subdirectory(drivers)

if (CONFIG_TRACE)
	add_subdirectory(trace)
endif()

if(CONFIG_LIBRARY)
	return()
endif()

add_subdirectory(debug)
add_subdirectory(init)
add_subdirectory(schedule)

if (CONFIG_PROBE)
	add_subdirectory(probe)
endif()

if (CONFIG_MULTICORE)
	add_subdirectory(idc)
endif()
