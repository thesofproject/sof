// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2020, Google Inc. All rights reserved
//
// Author: Curtis Malainey <cujomalainey@chromium.org>

#include <inttypes.h>
#include <stdlib.h>
#include <sof/drivers/ipc.h>
#include <sof/audio/component_ext.h>
#include <sof/lib/notifier.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/lib/agent.h>

static struct sof sof;

// fuzz_ipc.c
int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
	struct sof_ipc_cmd_hdr *hdr = (struct sof_ipc_cmd_hdr *)Data;
	// sanity check performed typically by platform dependent code
	if (hdr->size < sizeof(*hdr) || hdr->size > SOF_IPC_MSG_MAX_SIZE)
		return 0;

	ipc_cmd(hdr);
	return 0;  // Non-zero return values are reserved for future use.
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	/* init components */
	sys_comp_init(&sof);

	/* other necessary initializations, todo: follow better SOF init */
	pipeline_posn_init(&sof);
	init_system_notify(&sof);
	scheduler_init_edf();
	sa_init(&sof, CONFIG_SYSTICK_PERIOD);

	/* init IPC */
	if (ipc_init(&sof) < 0) {
		fprintf(stderr, "error: IPC init\n");
		exit(EXIT_FAILURE);
	}

	/* init scheduler */
	if (scheduler_init_edf() < 0) {
		fprintf(stderr, "error: edf scheduler init\n");
		exit(EXIT_FAILURE);
	}

	return 0;
}
