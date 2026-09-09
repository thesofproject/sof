/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Stub backend for acompressor.
 */

#include "acompressor.h"
#include <rtos/string.h>

static int acompressor_stub_init(struct processing_module *mod)
{
	return 0;
}

static int acompressor_stub_prepare(struct processing_module *mod)
{
	return 0;
}

static int acompressor_stub_process(struct processing_module *mod,
				    const void *src, void *dst, int frames,
				    enum sof_ipc_frame fmt)
{
	struct acompressor_comp_data *cd = module_get_private_data(mod);

	memcpy(dst, src, frames * cd->channels *
	       (fmt == SOF_IPC_FRAME_S16_LE ? sizeof(int16_t) : sizeof(int32_t)));
	return 0;
}

static int acompressor_stub_reset(struct processing_module *mod)
{
	return 0;
}

static int acompressor_stub_free(struct processing_module *mod)
{
	return 0;
}

const struct acompressor_backend acompressor_stub_backend = {
	.name = "stub",
	.init = acompressor_stub_init,
	.prepare = acompressor_stub_prepare,
	.process = acompressor_stub_process,
	.reset = acompressor_stub_reset,
	.free = acompressor_stub_free,
};
