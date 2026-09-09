/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Stub pass-through backend for alimiter module.
 */

#include "alimiter.h"
#include <rtos/string.h>

static int alimiter_stub_init(struct processing_module *mod)
{
	return 0;
}

static int alimiter_stub_prepare(struct processing_module *mod)
{
	return 0;
}

static int alimiter_stub_process(struct processing_module *mod,
				 const void *src, void *dst, int frames,
				 enum sof_ipc_frame fmt)
{
	struct alimiter_comp_data *cd = module_get_private_data(mod);
	size_t sample_bytes = (fmt == SOF_IPC_FRAME_S16_LE) ? sizeof(int16_t) : sizeof(int32_t);
	size_t nbytes = (size_t)frames * cd->channels * sample_bytes;

	memcpy(dst, src, nbytes);
	return 0;
}

static int alimiter_stub_reset(struct processing_module *mod)
{
	return 0;
}

static int alimiter_stub_free(struct processing_module *mod)
{
	return 0;
}

const struct alimiter_backend alimiter_stub_backend = {
	.name    = "alimiter_stub",
	.init    = alimiter_stub_init,
	.prepare = alimiter_stub_prepare,
	.process = alimiter_stub_process,
	.reset   = alimiter_stub_reset,
	.free    = alimiter_stub_free,
};
