# SPDX-License-Identifier: BSD-3-Clause

is_zephyr(zephyr)
if(zephyr)
	zephyr_library_sources_ifdef(CONFIG_SOF_USERSPACE_LL
		zephyr_ll_user.c
	)
endif()
