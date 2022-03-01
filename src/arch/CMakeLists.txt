# SPDX-License-Identifier: BSD-3-Clause

add_subdirectory(${ARCH})

if(NOT CONFIG_ZEPHYR_SOF_MODULE)
	add_subdirectory("xtos-wrapper")
endif()
