# SPDX-License-Identifier: BSD-3-Clause

# Code that only runs in kernel mode, see scripts/kernel_user_partition.txt.

# Zephyr adds this directory from zephyr/CMakeLists.txt, the host builds
# (testbench, host library) from src/CMakeLists.txt.

is_zephyr(zephyr)
if(zephyr)
	# Build everything below into the sof_kernel library created in
	# zephyr/CMakeLists.txt instead of modules_sof. This only sets the
	# variable for this directory and its subdirectories.
	set(ZEPHYR_CURRENT_LIBRARY sof_kernel)

	# Only built for Zephyr
	add_subdirectory(audio)
	add_subdirectory(debug)
	add_subdirectory(idc)
	add_subdirectory(init)
	add_subdirectory(library_manager)
	add_subdirectory(logging)
	add_subdirectory(probe)
	add_subdirectory(schedule)
endif()

# Built for Zephyr and for the host
add_subdirectory(ipc)
add_subdirectory(lib)
