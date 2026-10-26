# SPDX-License-Identifier: BSD-3-Clause

if(TESTBENCH_XTENSA_SIM OR CMAKE_C_COMPILER MATCHES "xt-|xtensa-" OR DEFINED ENV{XTENSA_CORE})
	target_compile_definitions(sof PRIVATE CONFIG_XT_SIMULATOR=1)
	add_local_sources(sof
		schedule.c
		ll_schedule.c
		dp_schedule.c
		edf_schedule.c
	)
else()
	add_local_sources(sof
		schedule.c
		ll_schedule_pthread.c
		dp_schedule_pthread.c
		edf_schedule.c
	)
endif()
