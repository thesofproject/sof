// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google Inc. All rights reserved.
//
// Author: Curtis Malainey <cujomalainey@chromium.org>

#include <sof/sof.h>
#include <sof/drivers/ipc.h>
#include <sof/drivers/timer.h>
#include <sof/lib/agent.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/lib/mailbox.h>

SHARED_DATA struct timer timer = {};

static uint8_t mailbox[MAILBOX_DSPBOX_SIZE +
		       MAILBOX_HOSTBOX_SIZE +
		       MAILBOX_EXCEPTION_SIZE +
		       MAILBOX_DEBUG_SIZE +
		       MAILBOX_STREAM_SIZE +
		       MAILBOX_TRACE_SIZE];

uint8_t *get_library_mailbox()
{
	return mailbox;
}

static void platform_clock_init(struct sof *sof) {}

static int dmac_init(struct sof *sof)
{
	return 0;
}

int platform_init(struct sof *sof)
{
	sof->platform_timer = &timer;
	sof->cpu_timers = &timer;

	platform_clock_init(sof);

	scheduler_init_edf();

	/* init low latency timer domain and scheduler */
	/* sof->platform_timer_domain = */
	/* timer_domain_init(sof->platform_timer, PLATFORM_DEFAULT_CLOCK, */
	/* CONFIG_SYSTICK_PERIOD); */

	/* init the system agent */
	sa_init(sof, CONFIG_SYSTICK_PERIOD);

	/* init DMACs */
	dmac_init(sof);

	/* initialise the host IPC mechanisms */
	ipc_init(sof);

	dai_init(sof);

	return 0;
}
