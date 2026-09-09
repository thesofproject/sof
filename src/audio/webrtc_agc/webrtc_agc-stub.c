/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Pass-through stub backend for webrtc_agc.
 */

#include "webrtc_agc.h"

#include <errno.h>

static int webrtc_agc_stub_init(struct processing_module *mod)
{
	return 0;
}

static int webrtc_agc_stub_prepare(struct processing_module *mod)
{
	return 0;
}

static int webrtc_agc_stub_process(struct processing_module *mod,
				   const void *src, void *dst, int frames,
				   enum sof_ipc_frame fmt)
{
	return -ENOSYS;
}

static int webrtc_agc_stub_reset(struct processing_module *mod)
{
	return 0;
}

static int webrtc_agc_stub_free(struct processing_module *mod)
{
	return 0;
}

const struct webrtc_agc_backend webrtc_agc_stub_backend = {
	.name    = "webrtc_agc_stub",
	.init    = webrtc_agc_stub_init,
	.prepare = webrtc_agc_stub_prepare,
	.process = webrtc_agc_stub_process,
	.reset   = webrtc_agc_stub_reset,
	.free    = webrtc_agc_stub_free,
};
