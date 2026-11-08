# SPDX-License-Identifier: BSD-3-Clause

is_zephyr(zephyr)
if(zephyr)
	zephyr_library_sources(
		zephyr_domain-kernel.c
	)

	# The kernel part of zephyr_ll.c, built with it, see src/schedule/CMakeLists.txt
	if(NOT CONFIG_SOC_ACP_6_0 OR CONFIG_DMA_DOMAIN)
		zephyr_library_sources(
			zephyr_ll-kernel.c
		)
	endif()

	zephyr_library_sources_ifdef(CONFIG_SOF_USERSPACE_LL
		zephyr_ll_user.c
	)
endif()
