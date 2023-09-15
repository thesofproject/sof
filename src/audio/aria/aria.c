// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Adrian Bonislawski <adrian.bonislawski@intel.com>

#include <sof/audio/aria/aria.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc4/aria.h>
#include <ipc4/fw_reg.h>
#include <ipc/dai.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(aria, CONFIG_SOF_LOG_LEVEL);

/* these ids aligns windows driver requirement to support windows driver */
/* 99f7166d-372c-43ef-81f6-22007aa15f03 */
DECLARE_SOF_RT_UUID("aria", aria_comp_uuid, 0x99f7166d, 0x372c, 0x43ef,
		    0x81, 0xf6, 0x22, 0x00, 0x7a, 0xa1, 0x5f, 0x03);

DECLARE_TR_CTX(aria_comp_tr, SOF_UUID(aria_comp_uuid), LOG_LEVEL_INFO);

static size_t get_required_emory(size_t chan_cnt, size_t smpl_group_cnt)
{
	/* Current implementation is able to apply 1 ms transition */
	/* internal circular buffer aligned to 8 bytes */
	const size_t num_of_ms = 1;

	return ALIGN_UP(num_of_ms * chan_cnt * smpl_group_cnt, 2) * sizeof(int32_t);
}

static int aria_algo_init(struct aria_data *cd, void *buffer_desc,
			  size_t att, size_t chan_cnt, size_t smpl_group_cnt)
{
	size_t idx;

	cd->chan_cnt = chan_cnt;
	cd->smpl_group_cnt = smpl_group_cnt;
	/* ensures buffer size is aligned to 8 bytes */
	cd->buff_size = ALIGN_UP(cd->chan_cnt * cd->smpl_group_cnt, 2);
	cd->offset = (cd->chan_cnt * cd->smpl_group_cnt) & 1;
	cd->att = att;
	cd->data_addr = buffer_desc;
	cd->data_ptr = cd->data_addr + cd->offset;
	cd->data_end = cd->data_addr + cd->buff_size;
	cd->buff_pos = 0;

	for (idx = 0; idx < ARIA_MAX_GAIN_STATES; ++idx)
		cd->gains[idx] = (1ULL << (32 - cd->att - 1)) - 1;

	memset((void *)cd->data_addr, 0, sizeof(int32_t) * cd->buff_size);
	cd->gain_state = 0;

	return 0;
}

static inline void aria_process_data(struct processing_module *mod,
				     struct audio_stream *source,
				     struct audio_stream *sink,
				     size_t frames)
{
	struct aria_data *cd = module_get_private_data(mod);
	size_t data_size = audio_stream_frame_bytes(source) * frames;
	size_t sample_size = audio_stream_get_channels(source) * frames;

	if (cd->att) {
		aria_algo_calc_gain(cd, INDEX_TAB[cd->gain_state + 1], source, frames);
		cd->aria_get_data(mod, sink, frames);
	} else {
		/* bypass processing gets unprocessed data from buffer */
		cir_buf_copy(cd->data_ptr, cd->data_addr, cd->data_end,
			     sink->w_ptr, sink->addr, sink->end_addr,
			     data_size);
	}

	cir_buf_copy(source->r_ptr, source->addr, source->end_addr,
		     cd->data_ptr, cd->data_addr, cd->data_end,
		     data_size);
	cd->data_ptr = cir_buf_wrap(cd->data_ptr + sample_size, cd->data_addr, cd->data_end);
}

int aria_init(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *mod_data = &mod->priv;
	struct ipc4_base_module_cfg *base_cfg = &mod_data->cfg.base_cfg;
	const struct ipc4_aria_module_cfg *aria = mod_data->cfg.init_data;
	struct aria_data *cd;
	size_t ibs, chc, sgs, sgc, req_mem, att;
	void *buf;

	comp_info(dev, "aria_init()");

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		return -ENOMEM;
	}

	ibs = base_cfg->ibs;
	chc = base_cfg->audio_fmt.channels_count;
	sgs = (base_cfg->audio_fmt.depth >> 3) * chc;
	sgc = ibs / sgs;
	req_mem = get_required_emory(chc, sgc);
	att = aria->attenuation;

	if (aria->attenuation > ARIA_MAX_ATT) {
		comp_warn(dev, "init_aria(): Attenuation value %d must not be greater than %d",
			  att, ARIA_MAX_ATT);
		att = ARIA_MAX_ATT;
	}
	mod_data->private = cd;

	buf = rballoc(0, SOF_MEM_CAPS_RAM, req_mem);

	if (!buf) {
		rfree(cd);
		comp_err(dev, "init_aria(): allocation failed for size %d", req_mem);
		return -ENOMEM;
	}

	return aria_algo_init(cd, buf, att, chc, sgc);
}

static int aria_free(struct processing_module *mod)
{
	struct aria_data *cd = module_get_private_data(mod);

	rfree(cd->data_addr);
	rfree(cd);
	return 0;
}

static void aria_set_stream_params(struct comp_buffer *buffer,
				   struct processing_module *mod)
{
	const struct ipc4_audio_format *audio_fmt = &mod->priv.cfg.base_cfg.audio_fmt;

	ipc4_update_buffer_format(buffer, audio_fmt);
#ifdef ARIA_GENERIC
	audio_stream_init_alignment_constants(1, 1, &buffer->stream);
#else
	audio_stream_init_alignment_constants(8, 1, &buffer->stream);
#endif
}

static int aria_prepare(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	int ret;
	struct comp_buffer *source, *sink;
	struct comp_dev *dev = mod->dev;
	struct aria_data *cd = module_get_private_data(mod);

	comp_info(dev, "aria_prepare()");

	source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	aria_set_stream_params(source, mod);

	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	aria_set_stream_params(sink, mod);

	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "aria_prepare(): Component is in active state.");
		return 0;
	}

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	cd->aria_get_data = aria_algo_get_data_func(mod);

	return 0;
}

static int aria_reset(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct aria_data *cd = module_get_private_data(mod);
	int idx;

	comp_info(dev, "aria_reset()");

	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "aria module is in active state. Ignore resetting");
		return 0;
	}

	for (idx = 0; idx < ARIA_MAX_GAIN_STATES; ++idx)
		cd->gains[idx] = (1ULL << (32 - cd->att - 1)) - 1;

	memset(cd->data_addr, 0, sizeof(int32_t) * cd->buff_size);
	cd->gain_state = 0;

	return 0;
}


static int aria_process(struct processing_module *mod,
			struct input_stream_buffer *input_buffers, int num_input_buffers,
			struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	/* Aria algo supports only 4-bytes containers */
	struct aria_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint32_t copy_bytes;
	uint32_t frames = input_buffers[0].size;

	comp_dbg(dev, "aria_copy()");

	frames = MIN(frames, cd->smpl_group_cnt);

	/* Aria won't change the stream format and channels, so sink and source
	 * has the same bytes to produce and consume.
	 */
	copy_bytes = frames * audio_stream_frame_bytes(input_buffers[0].data);
	if (copy_bytes == 0)
		return 0;

	aria_process_data(mod, input_buffers[0].data, output_buffers[0].data, frames);

	input_buffers[0].consumed = copy_bytes;
	output_buffers[0].size = copy_bytes;

	return 0;
}

static const struct module_interface aria_interface = {
	.init = aria_init,
	.prepare = aria_prepare,
	.process_audio_stream = aria_process,
	.reset = aria_reset,
	.free = aria_free
};

DECLARE_MODULE_ADAPTER(aria_interface, aria_comp_uuid, aria_comp_tr);
SOF_MODULE_INIT(aria, sys_comp_module_aria_interface_init);
