// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Dependency-free stub backend for the ffmpeg_dec module.
//
// This lets the SOF module glue (init/prepare/process/config/reset/free, the
// LLEXT manifest and the topology wiring) be built and exercised in CI without
// pulling in libavcodec. It does not decode: it simply passes the input bytes
// through to the output, so the pipeline moves data end to end. The real
// libavcodec implementation lives in ffmpeg_dec-ffmpeg.c and provides the same
// struct ffmpeg_dec_backend symbol.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/numbers.h>
#include <rtos/string.h>
#include <errno.h>
#include "ffmpeg_dec.h"

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

static int ffmpeg_dec_stub_init(struct processing_module *mod)
{
	comp_info(mod->dev, "ffmpeg_dec stub backend: no decoder linked");
	return 0;
}

static int ffmpeg_dec_stub_configure(struct processing_module *mod)
{
	return 0;
}

static int ffmpeg_dec_stub_decode(struct processing_module *mod,
				  const uint8_t *in, size_t in_size, size_t *consumed,
				  uint8_t *out, size_t out_size, size_t *produced)
{
	/* Passthrough: copy as many bytes as fit, report exact consume/produce. */
	size_t n = MIN(in_size, out_size);

	if (n)
		memcpy_s(out, out_size, in, n);

	*consumed = n;
	*produced = n;
	return 0;
}

static int ffmpeg_dec_stub_reset(struct processing_module *mod)
{
	return 0;
}

static int ffmpeg_dec_stub_free(struct processing_module *mod)
{
	return 0;
}

const struct ffmpeg_dec_backend ffmpeg_dec_backend = {
	.name = "stub",
	.init = ffmpeg_dec_stub_init,
	.configure = ffmpeg_dec_stub_configure,
	.decode = ffmpeg_dec_stub_decode,
	.reset = ffmpeg_dec_stub_reset,
	.free = ffmpeg_dec_stub_free,
};
