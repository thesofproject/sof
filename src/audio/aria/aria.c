// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Adrian Bonislawski <adrian.bonislawski@intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <rtos/init.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc4/fw_reg.h>
#include <ipc/dai.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include "aria.h"

#define ARIA_SET_ATTENUATION 1

LOG_MODULE_REGISTER(aria, CONFIG_SOF_LOG_LEVEL);

/* these ids aligns windows driver requirement to support windows driver */
SOF_DEFINE_REG_UUID(aria);

DECLARE_TR_CTX(aria_comp_tr, SOF_UUID(aria_uuid), LOG_LEVEL_INFO);

/**
 * \brief Aria gain index mapping table
 */
const int32_t sof_aria_index_tab[] = {
		0,    1,    2,    3,
		4,    5,    6,    7,
		8,    9,    0,    1,
		2,    3,    4,    5,
		6,    7,    8,    9,
		0,    1,    2,    3
};

static size_t get_required_emory(size_t chan_cnt, size_t smpl_group_cnt)
{
	/* Current implementation is able to apply 1 ms transition */
	/* internal circular buffer aligned to 8 bytes */
	const size_t num_of_ms = 1;

	return ALIGN_UP(num_of_ms * chan_cnt * smpl_group_cnt, 2) * sizeof(int32_t);
}

static void aria_set_gains(struct aria_data *cd)
{
	int i;

	for (i = 0; i < ARIA_MAX_GAIN_STATES; ++i)
		cd->gains[i] = (1ULL << (32 - cd->att - 1)) - 1;
}

static int aria_algo_init(struct aria_data *cd, void *buffer_desc,
			  size_t att, size_t chan_cnt, size_t smpl_group_cnt)
{
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

	aria_set_gains(cd);

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
		aria_algo_calc_gain(cd, sof_aria_index_tab[cd->gain_state + 1], source, frames);
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

static int aria_init(struct processing_module *mod)
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
#if SOF_USE_HIFI(3, ARIA) || SOF_USE_HIFI(4, ARIA)
	audio_stream_set_align(8, 1, &buffer->stream);
#elif SOF_USE_HIFI(5, ARIA)
	audio_stream_set_align(16, 1, &buffer->stream);
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

	source = comp_dev_get_first_data_producer(dev);
	aria_set_stream_params(source, mod);

	sink = comp_dev_get_first_data_consumer(dev);
	aria_set_stream_params(sink, mod);

	if (audio_stream_get_valid_fmt(&source->stream) != SOF_IPC_FRAME_S24_4LE ||
	    audio_stream_get_valid_fmt(&sink->stream) != SOF_IPC_FRAME_S24_4LE) {
		comp_err(dev, "aria_prepare(): format is not supported");
		return -EINVAL;
	}

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

static int aria_set_config(struct processing_module *mod, uint32_t param_id,
			   enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			   const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			   size_t response_size)
{
	struct aria_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_info(dev, "aria_set_config()");
	if (param_id == ARIA_SET_ATTENUATION) {
		if (fragment_size != sizeof(uint32_t)) {
			comp_err(dev, "Illegal fragment_size = %d", fragment_size);
			return -EINVAL;
		}
		memcpy_s(&cd->att, sizeof(uint32_t), fragment, sizeof(uint32_t));
		if (cd->att > ARIA_MAX_ATT) {
			comp_warn(dev,
				  "aria_set_config(): Attenuation parameter %d is limited to %d",
				  cd->att, ARIA_MAX_ATT);
			cd->att = ARIA_MAX_ATT;
		}
		aria_set_gains(cd);
	} else {
		comp_err(dev, "Illegal param_id = %d", param_id);
		return -EINVAL;
	}

	return 0;
}

static const struct module_interface aria_interface = {
	.init = aria_init,
	.prepare = aria_prepare,
	.process_audio_stream = aria_process,
	.reset = aria_reset,
	.free = aria_free,
	.set_configuration = aria_set_config,
};

DECLARE_MODULE_ADAPTER(aria_interface, aria_uuid, aria_comp_tr);
SOF_MODULE_INIT(aria, sys_comp_module_aria_interface_init);

#if CONFIG_COMP_ARIA_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

SOF_LLEXT_MOD_ENTRY(aria, &aria_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("ARIA", aria_llext_entry, 1, SOF_REG_UUID(aria), 8);

SOF_LLEXT_BUILDINFO;

#endif
