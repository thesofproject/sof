/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Stub backend for equalizer.
 */

#include "equalizer.h"
#include <rtos/string.h>

static int equalizer_stub_init(struct processing_module *mod)
{
	return 0;
}

static int equalizer_stub_prepare(struct processing_module *mod)
{
	return 0;
}

static int equalizer_stub_process(struct processing_module *mod,
				  const void *src, void *dst, int frames,
				  enum sof_ipc_frame fmt)
{
	struct equalizer_comp_data *cd = module_get_private_data(mod);

	memcpy(dst, src, frames * cd->channels *
	       (fmt == SOF_IPC_FRAME_S16_LE ? sizeof(int16_t) : sizeof(int32_t)));
	return 0;
}

static int equalizer_stub_reset(struct processing_module *mod)
{
	return 0;
}

static int equalizer_stub_free(struct processing_module *mod)
{
	return 0;
}

const struct equalizer_backend equalizer_stub_backend = {
	.name = "stub",
	.init = equalizer_stub_init,
	.prepare = equalizer_stub_prepare,
	.process = equalizer_stub_process,
	.reset = equalizer_stub_reset,
	.free = equalizer_stub_free,
};
