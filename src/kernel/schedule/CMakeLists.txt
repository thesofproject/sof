# SPDX-License-Identifier: BSD-3-Clause

is_zephyr(zephyr)
if(zephyr)
	zephyr_library_sources(
		schedule-kernel.c
		zephyr_domain-kernel.c
		zephyr_ll-kernel.c
	)

	zephyr_library_sources_ifdef(CONFIG_SOF_USERSPACE_LL
		zephyr_ll_user.c
	)

	zephyr_library_sources_ifdef(CONFIG_ZEPHYR_DP_SCHEDULER
		zephyr_dp_schedule-kernel.c
	)
	if(CONFIG_SOF_USERSPACE_APPLICATION)
		zephyr_library_sources_ifdef(CONFIG_ZEPHYR_DP_SCHEDULER
			zephyr_dp_schedule_application-kernel.c
		)
	else()
		zephyr_library_sources_ifdef(CONFIG_ZEPHYR_DP_SCHEDULER
			zephyr_dp_schedule_thread-kernel.c
		)
	endif()

	zephyr_library_sources_ifdef(CONFIG_ZEPHYR_TWB_SCHEDULER
		zephyr_twb_schedule.c
	)
endif()
