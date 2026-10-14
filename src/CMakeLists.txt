# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(platform)
add_subdirectory(arch)
add_subdirectory(ipc)
add_subdirectory(audio)
add_subdirectory(lib)
add_subdirectory(math)
add_subdirectory(module)

if(CONFIG_SAMPLES)
	add_subdirectory(samples)
endif()

add_subdirectory(drivers)

if (CONFIG_TRACE)
	add_subdirectory(trace)
endif()
