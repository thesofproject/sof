// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Google LLC.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/component.h>
#include <sof/trace/trace.h>
#include <sof/ipc/msg.h>

#include <sof/trace/trace.h>
#include <user/trace.h>
#include <sof/bit.h>
#include <sof/common.h>
#include <stdint.h>
#include <stdlib.h>
#include <sof/audio/smart_amp/smart_amp.h>

/* self-declared inner model data struct */
struct passthru_mod_data {
	struct smart_amp_mod_data_base base;
	uint16_t ff_fmt;
	uint16_t fb_fmt;
};

static const int supported_fmt_count = 3;
static const uint16_t supported_fmts[] = {
	SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE
};

/**
 * mod ops implementation.
 */

static int passthru_mod_init(struct smart_amp_mod_data_base *mod)
{
	comp_info(mod->dev, "[PassThru Amp] init");
	return 0;
}

static int passthru_mod_query_memblk_size(struct smart_amp_mod_data_base *mod,
					  enum smart_amp_mod_memblk blk)
{
	return 0;
}

static int passthru_mod_set_memblk(struct smart_amp_mod_data_base *mod,
				   enum smart_amp_mod_memblk blk,
				   struct smart_amp_buf *buf)
{
	return 0;
}

static int passthru_mod_get_supported_fmts(struct smart_amp_mod_data_base *mod,
					   const uint16_t **mod_fmts, int *num_mod_fmts)
{
	*num_mod_fmts = supported_fmt_count;
	*mod_fmts = supported_fmts;
	return 0;
}

static int passthru_mod_set_fmt(struct smart_amp_mod_data_base *mod, uint16_t mod_fmt)
{
	struct passthru_mod_data *pmd = (struct passthru_mod_data *)mod;

	comp_info(mod->dev, "[PassThru Amp] set fmt:%u", mod_fmt);
	pmd->ff_fmt = mod_fmt;
	pmd->fb_fmt = mod_fmt;
	return 0;
}

static int passthru_mod_ff_proc(struct smart_amp_mod_data_base *mod,
				uint32_t frames,
				struct smart_amp_mod_stream *in,
				struct smart_amp_mod_stream *out)
{
	struct passthru_mod_data *pmd = (struct passthru_mod_data *)mod;
	bool is_16bit = (pmd->ff_fmt == SOF_IPC_FRAME_S16_LE);
	uint32_t szsample = (is_16bit ? 2 : 4);
	uint32_t size = frames * in->channels * szsample;

	comp_dbg(mod->dev, "[PassThru Amp] bypass %u frames", frames);

	/* passthrough all frames */
	memcpy_s(out->buf.data, out->buf.size, in->buf.data, size);
	in->consumed = frames;
	out->produced = frames;
	return 0;
}

static int passthru_mod_fb_proc(struct smart_amp_mod_data_base *mod,
				uint32_t frames,
				struct smart_amp_mod_stream *in)
{
	in->consumed = frames;
	return 0;
}

static int passthru_mod_get_config(struct smart_amp_mod_data_base *mod,
				   struct sof_ipc_ctrl_data *cdata, uint32_t size)
{
	cdata->data->abi = SOF_ABI_VERSION;
	cdata->data->size = 0;
	return 0;
}

static int passthru_mod_set_config(struct smart_amp_mod_data_base *mod,
				   struct sof_ipc_ctrl_data *cdata)
{
	return 0;
}

static int passthru_mod_reset(struct smart_amp_mod_data_base *mod)
{
	comp_info(mod->dev, "[PassThru Amp] reset");
	return 0;
}

static const struct inner_model_ops passthru_mod_ops = {
	.init = passthru_mod_init,
	.query_memblk_size = passthru_mod_query_memblk_size,
	.set_memblk = passthru_mod_set_memblk,
	.get_supported_fmts = passthru_mod_get_supported_fmts,
	.set_fmt = passthru_mod_set_fmt,
	.ff_proc = passthru_mod_ff_proc,
	.fb_proc = passthru_mod_fb_proc,
	.set_config = passthru_mod_set_config,
	.get_config = passthru_mod_get_config,
	.reset = passthru_mod_reset
};

/**
 * mod_data_create() implementation.
 */

struct smart_amp_mod_data_base *mod_data_create(const struct comp_dev *dev)
{
	struct passthru_mod_data *mod;

	mod = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*mod));
	if (!mod)
		return NULL;

	mod->base.dev = dev;
	mod->base.mod_ops = &passthru_mod_ops;
	return &mod->base;
}
