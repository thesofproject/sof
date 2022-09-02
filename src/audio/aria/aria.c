// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Adrian Bonislawski <adrian.bonislawski@intel.com>

#include <sof/audio/aria/aria.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
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

static const struct comp_driver comp_aria;

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

static int aria_algo_init(struct comp_dev *dev, void *buffer_desc, void *buf_in,
			  void *buf_out, size_t att, size_t chan_cnt, size_t smpl_group_cnt)
{
	struct aria_data *cd = comp_get_drvdata(dev);
	size_t idx;

	cd->chan_cnt = chan_cnt;
	cd->smpl_group_cnt = smpl_group_cnt;
	/* ensures buffer size is aligned to 8 bytes */
	cd->buff_size = ALIGN_UP(cd->chan_cnt * cd->smpl_group_cnt, 2);
	cd->offset = (cd->chan_cnt * cd->smpl_group_cnt) % 2;
	cd->att = att;
	cd->data = buffer_desc;
	cd->buf_in = buf_in;
	cd->buf_out = buf_out;
	cd->buff_pos = 0;

	for (idx = 0; idx < ARIA_MAX_GAIN_STATES; ++idx)
		cd->gains[idx] = (1ULL << (32 - cd->att - 1)) - 1;

	memset((void *)cd->data, 0, sizeof(int32_t) * cd->buff_size);
	memset((void *)cd->buf_in, 0, sizeof(int32_t) * cd->buff_size);
	memset((void *)cd->buf_out, 0, sizeof(int32_t) * cd->buff_size);
	cd->gain_state = 0;

	return 0;
}

int aria_algo_buffer_data(struct comp_dev *dev, int32_t *__restrict data, size_t size)
{
	struct aria_data *cd = comp_get_drvdata(dev);
	size_t min_buff = MIN(cd->buff_size - cd->buff_pos - cd->offset, size);
	int ret;

	ret = memcpy_s(&cd->data[cd->buff_pos], cd->buff_size * sizeof(int32_t),
		       data, min_buff * sizeof(int32_t));
	if (ret < 0)
		return ret;
	ret = memcpy_s(cd->data, cd->buff_size * sizeof(int32_t),
		       &data[min_buff], (size - min_buff) * sizeof(int32_t));
	if (ret < 0)
		return ret;
	cd->buff_pos = (cd->buff_pos + size) % cd->buff_size;

	return 0;
}

int aria_process_data(struct comp_dev *dev,
		      int32_t *__restrict dst, size_t dst_size,
		      int32_t *__restrict src, size_t src_size)
{
	struct aria_data *cd = comp_get_drvdata(dev);
	size_t min_buff;
	int ret;

	if (cd->att) {
		aria_algo_calc_gain(dev, (cd->gain_state + 1) % ARIA_MAX_GAIN_STATES,
				    src, src_size);
		aria_algo_get_data(dev, dst, dst_size);
	} else {
		/* bypass processing gets unprocessed data from buffer */
		min_buff = MIN(cd->buff_size - cd->buff_pos - cd->offset, dst_size);
		ret = memcpy_s(dst, cd->buff_size * sizeof(int32_t),
			       &cd->data[cd->buff_pos + cd->offset],
			       min_buff * sizeof(int32_t));
		if (ret < 0)
			return ret;
		ret = memcpy_s(&dst[min_buff], cd->buff_size * sizeof(int32_t),
			       cd->data, (dst_size - min_buff) * sizeof(int32_t));
		if (ret < 0)
			return ret;
	}
	return aria_algo_buffer_data(dev, src, src_size);
}

static int init_aria(struct comp_dev *dev, struct comp_ipc_config *config,
		     void *spec)
{
	struct ipc4_aria_module_cfg *aria = spec;
	struct aria_data *cd;
	size_t ibs, chc, sgs, sgc, req_mem, att;
	void *buf, *buf_in, *buf_out;
	int ret;

	dev->ipc_config = *config;
	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	dcache_invalidate_region((__sparse_force void __sparse_cache *)spec, sizeof(*aria));
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		comp_free(dev);
		return -ENOMEM;
	}

	/* Copy received data format to local structures */
	ret = memcpy_s(&cd->base, sizeof(struct ipc4_base_module_cfg),
		       &aria->base_cfg, sizeof(struct ipc4_base_module_cfg));
	if (ret < 0)
		return ret;

	ibs = cd->base.ibs;
	chc = cd->base.audio_fmt.channels_count;
	sgs = (cd->base.audio_fmt.valid_bit_depth >> 3) * chc;
	sgc = ibs / sgs;
	req_mem = get_required_emory(chc, sgc);
	att = aria->attenuation;

	if (aria->attenuation > ARIA_MAX_ATT) {
		comp_warn(dev, "init_aria(): Attenuation value %d must not be greater than %d",
			  att, ARIA_MAX_ATT);
		att = ARIA_MAX_ATT;
	}

	comp_set_drvdata(dev, cd);

	buf = rballoc(0, SOF_MEM_CAPS_RAM, req_mem);
	buf_in = rballoc(0, SOF_MEM_CAPS_RAM, req_mem);
	buf_out = rballoc(0, SOF_MEM_CAPS_RAM, req_mem);

	if (!buf || !buf_in || !buf_out) {
		rfree(buf);
		rfree(buf_in);
		rfree(buf_out);
		rfree(cd);
		comp_free(dev);
		comp_err(dev, "init_aria(): allocation failed for size %d", req_mem);
		return -ENOMEM;
	}

	return aria_algo_init(dev, buf, buf_in, buf_out, att, chc, sgc);
}

static struct comp_dev *aria_new(const struct comp_driver *drv,
				 struct comp_ipc_config *config, void *spec)
{
	struct comp_dev *dev;
	int ret;

	comp_cl_info(&comp_aria, "aria_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	ret = init_aria(dev, config, spec);
	if (ret < 0) {
		comp_free(dev);
		return NULL;
	}
	dev->state = COMP_STATE_READY;

	return dev;
}

static void aria_free(struct comp_dev *dev)
{
	struct aria_data *cd = comp_get_drvdata(dev);

	rfree(cd->data);
	rfree(cd->buf_in);
	rfree(cd->buf_out);
	rfree(cd);
	rfree(dev);
}

static int aria_prepare(struct comp_dev *dev)
{
	int ret;

	comp_info(dev, "aria_prepare()");

	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "aria_prepare(): Component is in active state.");
		return 0;
	}

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return 0;
}

static int aria_reset(struct comp_dev *dev)
{
	struct aria_data *cd = comp_get_drvdata(dev);
	int idx;

	comp_info(dev, "aria_reset()");

	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "aria module is in active state. Ignore resetting");
		return 0;
	}

	for (idx = 0; idx < ARIA_MAX_GAIN_STATES; ++idx)
		cd->gains[idx] = (1ULL << (32 - cd->att - 1)) - 1;

	memset(cd->data, 0, sizeof(int32_t) * cd->buff_size);
	memset(cd->buf_in, 0, sizeof(int32_t) * cd->buff_size);
	memset(cd->buf_out, 0, sizeof(int32_t) * cd->buff_size);
	cd->gain_state = 0;

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

static int aria_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "aria_trigger()");

	return comp_set_state(dev, cmd);
}

static int aria_copy(struct comp_dev *dev)
{
	struct comp_copy_limits c;
	struct comp_buffer *source, *sink;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	struct aria_data *cd;
	uint32_t source_bytes;
	uint32_t sink_bytes;
	uint32_t frag = 0;
	int32_t *destp;
	int32_t i, channel;

	cd = comp_get_drvdata(dev);

	comp_dbg(dev, "aria_copy()");

	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	source_c = buffer_acquire(source);
	sink_c = buffer_acquire(sink);

	comp_get_copy_limits(source_c, sink_c, &c);
	source_bytes = c.frames * c.source_frame_bytes;
	sink_bytes = c.frames * c.sink_frame_bytes;

	if (source_bytes == 0)
		goto out;

	buffer_stream_invalidate(source_c, source_bytes);

	for (i = 0; i < c.frames; i++) {
		for (channel = 0; channel < sink_c->stream.channels; channel++) {
			cd->buf_in[frag] = *(int32_t *)audio_stream_read_frag_s32(&source_c->stream,
										  frag);

			frag++;
		}
	}
	dcache_writeback_region((__sparse_force void __sparse_cache *)cd->buf_in,
				c.frames * sink_c->stream.channels * sizeof(int32_t));

	aria_process_data(dev, cd->buf_out, sink_bytes / sizeof(uint32_t),
			  cd->buf_in, source_bytes / sizeof(uint32_t));

	dcache_writeback_region((__sparse_force void __sparse_cache *)cd->buf_out,
				c.frames * sink_c->stream.channels * sizeof(int32_t));
	frag = 0;
	for (i = 0; i < c.frames; i++) {
		for (channel = 0; channel < sink_c->stream.channels; channel++) {
			destp = audio_stream_write_frag_s32(&sink_c->stream, frag);
			*destp = cd->buf_out[frag];

			frag++;
		}
	}

	buffer_stream_writeback(sink_c, sink_bytes);

	comp_update_buffer_produce(sink_c, sink_bytes);
	comp_update_buffer_consume(source_c, source_bytes);

out:
	buffer_release(sink_c);
	buffer_release(source_c);

	return 0;
}

static int aria_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct aria_data *cd = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		*(struct ipc4_base_module_cfg *)value = cd->base;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct comp_driver comp_aria = {
	.uid	= SOF_RT_UUID(aria_comp_uuid),
	.tctx	= &aria_comp_tr,
	.ops	= {
		.create			= aria_new,
		.free			= aria_free,
		.trigger		= aria_trigger,
		.copy			= aria_copy,
		.prepare		= aria_prepare,
		.reset			= aria_reset,
		.get_attribute		= aria_get_attribute,
	},
};

static SHARED_DATA struct comp_driver_info comp_aria_info = {
	.drv = &comp_aria,
};

UT_STATIC void sys_comp_aria_init(void)
{
	comp_register(platform_shared_get(&comp_aria_info,
					  sizeof(comp_aria_info)));
}

DECLARE_MODULE(sys_comp_aria_init);
