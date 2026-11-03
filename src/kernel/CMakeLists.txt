# SPDX-License-Identifier: BSD-3-Clause

# Code that only runs in kernel mode, see scripts/kernel_user_partition.txt.

is_zephyr(zephyr)
if(zephyr)
	# Build everything below into the sof_kernel library created in
	# zephyr/CMakeLists.txt instead of modules_sof. This only sets the
	# variable for this directory and its subdirectories.
	set(ZEPHYR_CURRENT_LIBRARY sof_kernel)
endif()

add_subdirectory(idc)
add_subdirectory(init)
