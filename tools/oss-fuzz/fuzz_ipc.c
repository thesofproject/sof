// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2020, Google Inc. All rights reserved
//
// Author: Curtis Malainey <cujomalainey@chromium.org>

#include <inttypes.h>
#include <stdlib.h>
#include <sof/drivers/ipc.h>
#include <sof/math/numbers.h>
#include <sof/audio/component_ext.h>
#include <sof/lib/notifier.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/lib/agent.h>

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size);
int LLVMFuzzerInitialize(int *argc, char ***argv);
// fuzz_ipc.c
int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
	// since we can always assume the mailbox is allocated
	// copy the buffer to pre allocated buffer
	struct sof_ipc_cmd_hdr *hdr = malloc(SOF_IPC_MSG_MAX_SIZE);

	memcpy_s(hdr, SOF_IPC_MSG_MAX_SIZE, Data, MIN(Size, SOF_IPC_MSG_MAX_SIZE));

	// sanity check performed typically by platform dependent code
	if (hdr->size < sizeof(*hdr) || hdr->size > SOF_IPC_MSG_MAX_SIZE)
		goto done;

	ipc_cmd(hdr);
done:
	free(hdr);
	return 0;  // Non-zero return values are reserved for future use.
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	/* init components */
	sys_comp_init(sof_get());

	/* other necessary initializations, todo: follow better SOF init */
	pipeline_posn_init(sof_get());
	init_system_notify(sof_get());
	scheduler_init_edf();
	sa_init(sof_get(), CONFIG_SYSTICK_PERIOD);

	/* init IPC */
	if (ipc_init(sof_get()) < 0) {
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
