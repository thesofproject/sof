/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Stub backend for stereowiden.
 */

#include "stereowiden.h"
#include <rtos/string.h>

static int stereowiden_stub_init(struct processing_module *mod)
{
	return 0;
}

static int stereowiden_stub_prepare(struct processing_module *mod)
{
	return 0;
}

static int stereowiden_stub_process(struct processing_module *mod,
				    const void *src, void *dst, int frames,
				    enum sof_ipc_frame fmt)
{
	struct stereowiden_comp_data *cd = module_get_private_data(mod);

	memcpy(dst, src, frames * cd->channels *
	       (fmt == SOF_IPC_FRAME_S16_LE ? sizeof(int16_t) : sizeof(int32_t)));
	return 0;
}

static int stereowiden_stub_reset(struct processing_module *mod)
{
	return 0;
}

static int stereowiden_stub_free(struct processing_module *mod)
{
	return 0;
}

const struct stereowiden_backend stereowiden_stub_backend = {
	.name = "stub",
	.init = stereowiden_stub_init,
	.prepare = stereowiden_stub_prepare,
	.process = stereowiden_stub_process,
	.reset = stereowiden_stub_reset,
	.free = stereowiden_stub_free,
};
