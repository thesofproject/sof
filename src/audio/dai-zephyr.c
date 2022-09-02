// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <rtos/interrupt.h>
#include <sof/ipc/msg.h>
#include <rtos/interrupt.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/dai.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/dai.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/dai.h>

static const struct comp_driver comp_dai;

LOG_MODULE_REGISTER(dai_comp, CONFIG_SOF_LOG_LEVEL);

/* c2b00d27-ffbc-4150-a51a-245c79c5e54b */
DECLARE_SOF_RT_UUID("dai", dai_comp_uuid, 0xc2b00d27, 0xffbc, 0x4150,
		    0xa5, 0x1a, 0x24, 0x5c, 0x79, 0xc5, 0xe5, 0x4b);

DECLARE_TR_CTX(dai_comp_tr, SOF_UUID(dai_comp_uuid), LOG_LEVEL_INFO);

#if CONFIG_COMP_DAI_GROUP

static int dai_comp_trigger_internal(struct comp_dev *dev, int cmd);

static void dai_atomic_trigger(void *arg, enum notify_id type, void *data)
{
	struct comp_dev *dev = arg;
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dai_group *group = dd->group;

	/* Atomic context set by the last DAI to receive trigger command */
	group->trigger_ret = dai_comp_trigger_internal(dev, group->trigger_cmd);
}

/* Assign DAI to a group */
int dai_assign_group(struct comp_dev *dev, uint32_t group_id)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	if (dd->group) {
		if (dd->group->group_id != group_id) {
			comp_err(dev, "dai_assign_group(), DAI already in group %d, requested %d",
				 dd->group->group_id, group_id);
			return -EINVAL;
		}

		/* No need to re-assign to the same group, do nothing */
		return 0;
	}

	dd->group = dai_group_get(group_id, DAI_CREAT);
	if (!dd->group) {
		comp_err(dev, "dai_assign_group(), failed to assign group %d",
			 group_id);
		return -EINVAL;
	}

	comp_dbg(dev, "dai_assign_group(), group %d num %d",
		 group_id, dd->group->num_dais);

	/* Register for the atomic trigger event */
	notifier_register(dev, dd->group, NOTIFIER_ID_DAI_TRIGGER,
			  dai_atomic_trigger, 0);

	return 0;
}
#endif

static int dai_trigger_op(struct dai *dai, int cmd, int direction)
{
	const struct device *dev = dai->dev;
	enum dai_trigger_cmd zephyr_cmd;

	switch (cmd) {
	case COMP_TRIGGER_STOP:
		zephyr_cmd = DAI_TRIGGER_STOP;
		break;
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		zephyr_cmd = DAI_TRIGGER_START;
		break;
	case COMP_TRIGGER_PAUSE:
		zephyr_cmd = DAI_TRIGGER_PAUSE;
		break;
	case COMP_TRIGGER_PRE_START:
	case COMP_TRIGGER_PRE_RELEASE:
		zephyr_cmd = DAI_TRIGGER_PRE_START;
		break;
	default:
		return -EINVAL;
	}

	return dai_trigger(dev, direction, zephyr_cmd);
}

/* called from src/ipc/ipc3/handler.c and src/ipc/ipc4/dai.c */
int dai_set_config(struct dai *dai, struct ipc_config_dai *common_config,
		   void *spec_config)
{
	const struct device *dev = dai->dev;
	struct sof_ipc_dai_config *sof_cfg;
	struct dai_config cfg;
	void *cfg_params;
	bool is_blob;

	sof_cfg = spec_config;
	cfg.dai_index = common_config->dai_index;
	is_blob = common_config->is_config_blob;
	cfg.format = sof_cfg->format;
	cfg.options = sof_cfg->flags;
	cfg.rate = common_config->sampling_frequency;

	switch (common_config->type) {
	case SOF_DAI_INTEL_SSP:
		cfg.type = is_blob ? DAI_INTEL_SSP_NHLT : DAI_INTEL_SSP;
		cfg_params = is_blob ? spec_config : &sof_cfg->ssp;
		break;
	case SOF_DAI_INTEL_ALH:
		cfg.type = is_blob ? DAI_INTEL_ALH_NHLT : DAI_INTEL_ALH;
		cfg_params = is_blob ? spec_config : &sof_cfg->alh;
		break;
	case SOF_DAI_INTEL_DMIC:
		cfg.type = is_blob ? DAI_INTEL_DMIC_NHLT : DAI_INTEL_DMIC;
		cfg_params = is_blob ? spec_config : &sof_cfg->dmic;
		break;
	case SOF_DAI_INTEL_HDA:
		cfg.type = is_blob ? DAI_INTEL_HDA_NHLT : DAI_INTEL_HDA;
		cfg_params = is_blob ? spec_config : &sof_cfg->hda;
		break;
	default:
		return -EINVAL;
	}

	return dai_config_set(dev, &cfg, cfg_params);
}

static int dai_get_hw_params(struct dai *dai, struct sof_ipc_stream_params  *params, int dir)
{
	const struct dai_config *cfg = dai_config_get(dai->dev, dir);

	params->rate = cfg->rate;
	params->buffer_fmt = 0;
	params->channels = cfg->channels;

	switch (cfg->word_size) {
	case 16:
		params->frame_fmt = SOF_IPC_FRAME_S16_LE;
		break;
	case 24:
		params->frame_fmt = SOF_IPC_FRAME_S24_4LE;
		break;
	case 32:
		params->frame_fmt = SOF_IPC_FRAME_S32_LE;
		break;
	default:
		tr_warn(&dai_comp_tr, "not supported word size %u", cfg->word_size);
	}

	return 0;
}

/* called from ipc/ipc3/dai.c */
int dai_get_handshake(struct dai *dai, int direction, int stream_id)
{
	const struct dai_properties *props = dai_get_properties(dai->dev, direction, stream_id);

	return props->dma_hs_id;
}

/* called from ipc/ipc3/dai.c and ipc/ipc4/dai.c */
int dai_get_fifo_depth(struct dai *dai, int direction)
{
	const struct dai_properties *props = dai_get_properties(dai->dev, direction, 0);

	return props->fifo_depth;
}

int dai_get_stream_id(struct dai *dai, int direction)
{
	const struct dai_properties *props = dai_get_properties(dai->dev, direction, 0);

	return props->stream_id;
}

static int dai_get_fifo(struct dai *dai, int direction, int stream_id)
{
	const struct dai_properties *props = dai_get_properties(dai->dev, direction, stream_id);

	return props->fifo_address;
}

/* this is called by DMA driver every time descriptor has completed */
static void dai_dma_cb(void *arg, enum notify_id type, void *data)
{
	struct dma_cb_data *next = data;
	struct comp_dev *dev = arg;
	struct dai_data *dd = comp_get_drvdata(dev);
	uint32_t bytes = next->elem.size;
	struct comp_buffer __sparse_cache *local_buf, *dma_buf;
	void *buffer_ptr;
	int ret;

	comp_dbg(dev, "dai_dma_cb()");

	next->status = DMA_CB_STATUS_RELOAD;

	/* stop dma copy for pause/stop/xrun */
	if (dev->state != COMP_STATE_ACTIVE || dd->xrun) {
		/* stop the DAI */
		dai_trigger_op(dd->dai, COMP_TRIGGER_STOP, dev->direction);

		/* tell DMA not to reload */
		next->status = DMA_CB_STATUS_END;
	}

	dma_buf = buffer_acquire(dd->dma_buffer);

	/* is our pipeline handling an XRUN ? */
	if (dd->xrun) {
		/* make sure we only playback silence during an XRUN */
		if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
			/* fill buffer with silence */
			buffer_zero(dma_buf);
		buffer_release(dma_buf);

		return;
	}

	local_buf = buffer_acquire(dd->local_buffer);

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		ret = dma_buffer_copy_to(local_buf, dma_buf,
					 dd->process, bytes);

		buffer_ptr = local_buf->stream.r_ptr;
	} else {
		ret = dma_buffer_copy_from(dma_buf, local_buf,
					   dd->process, bytes);

		buffer_ptr = local_buf->stream.w_ptr;
	}

	/* assert dma_buffer_copy succeed */
	if (ret < 0) {
		struct comp_buffer __sparse_cache *source_c, *sink_c;

		source_c = dev->direction == SOF_IPC_STREAM_PLAYBACK ?
					local_buf : dma_buf;
		sink_c = dev->direction == SOF_IPC_STREAM_PLAYBACK ?
					dma_buf : local_buf;
		comp_err(dev, "dai_dma_cb() dma buffer copy failed, dir %d bytes %d avail %d free %d",
			 dev->direction, bytes,
			 audio_stream_get_avail_samples(&source_c->stream) *
				audio_stream_frame_bytes(&source_c->stream),
			 audio_stream_get_free_samples(&sink_c->stream) *
				audio_stream_frame_bytes(&sink_c->stream));
	} else {
		/* update host position (in bytes offset) for drivers */
		dd->total_data_processed += bytes;
	}

	buffer_release(local_buf);
	buffer_release(dma_buf);
}

static struct comp_dev *dai_new(const struct comp_driver *drv,
				struct comp_ipc_config *config,
				void *spec)
{
	struct comp_dev *dev;
	struct ipc_config_dai *dai_cfg = spec;
	struct dai_data *dd;
	uint32_t dir;

	comp_cl_dbg(&comp_dai, "dai_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	dd = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*dd));
	if (!dd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, dd);

	dd->dai = dai_get(dai_cfg->type, dai_cfg->dai_index, DAI_CREAT);
	if (!dd->dai) {
		comp_cl_err(&comp_dai, "dai_new(): dai_get() failed to create DAI.");
		goto error;
	}

	dd->ipc_config = *dai_cfg;

	/* request GP LP DMA with shared access privilege */
	dir = dai_cfg->direction == SOF_IPC_STREAM_PLAYBACK ?
		DMA_DIR_MEM_TO_DEV : DMA_DIR_DEV_TO_MEM;

	dd->dma = dma_get(dir, dd->dai->dma_caps, dd->dai->dma_dev, DMA_ACCESS_SHARED);
	if (!dd->dma) {
		comp_cl_err(&comp_dai, "dai_new(): dma_get() failed to get shared access to DMA.");
		goto error;
	}

	dma_sg_init(&dd->config.elem_array);
	dd->xrun = 0;
	dd->chan = NULL;

	dev->state = COMP_STATE_READY;
	return dev;

error:
	rfree(dd);
	rfree(dev);
	return NULL;
}

static void dai_free(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	if (dd->group) {
		notifier_unregister(dev, dd->group, NOTIFIER_ID_DAI_TRIGGER);
		dai_group_put(dd->group);
	}

	if (dd->chan) {
		notifier_unregister(dev, dd->chan, NOTIFIER_ID_DMA_COPY);
		dma_release_channel(dd->dma->z_dev, dd->chan->index);
		dd->chan->dev_data = NULL;
	}

	dma_put(dd->dma);

	dai_put(dd->dai);

	rfree(dd->dai_spec_config);
	rfree(dd);
	rfree(dev);
}

static int dai_comp_get_hw_params(struct comp_dev *dev,
				  struct sof_ipc_stream_params *params,
				  int dir)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	int ret;

	comp_dbg(dev, "dai_hw_params()");

	/* fetching hw dai stream params */
	ret = dai_get_hw_params(dd->dai, params, dir);
	if (ret < 0) {
		comp_err(dev, "dai_comp_get_hw_params(): dai_get_hw_params failed ret %d",
			 ret);
		return ret;
	}

	/* dai_comp_get_hw_params() function fetches hardware dai parameters,
	 * which then are propagating back through the pipeline, so that any
	 * component can convert specific stream parameter. Here, we overwrite
	 * frame_fmt hardware parameter as DAI component is able to convert
	 * stream with different frame_fmt's (using pcm converter)
	 */
	params->frame_fmt = dev->ipc_config.frame_fmt;

	return 0;
}

static int dai_comp_hw_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	return 0;
}

static int dai_verify_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	struct sof_ipc_stream_params hw_params;

	dai_comp_get_hw_params(dev, &hw_params, params->direction);

	/* checks whether pcm parameters match hardware DAI parameter set
	 * during dai_set_config(). If hardware parameter is equal to 0, it
	 * means that it can vary, so any value is acceptable. We do not check
	 * format parameter, because DAI is able to change format using
	 * pcm_converter functions.
	 */
	if (hw_params.rate && hw_params.rate != params->rate) {
		comp_err(dev, "dai_verify_params(): pcm rate parameter %d does not match hardware rate %d",
			 params->rate, hw_params.rate);
		return -EINVAL;
	}

	if (hw_params.channels && hw_params.channels != params->channels) {
		comp_err(dev, "dai_verify_params(): pcm channels parameter %d does not match hardware channels %d",
			 params->channels, hw_params.channels);
		return -EINVAL;
	}

	/* set component period frames */
	component_set_nearest_period_frames(dev, params->rate);

	return 0;
}

/* set component audio SSP and DMA configuration */
static int dai_playback_params(struct comp_dev *dev, uint32_t period_bytes,
			       uint32_t period_count)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	struct dma_config *dma_cfg;
	struct dma_block_config *dma_block_cfg;
	struct dma_block_config *prev = NULL;
	struct comp_buffer __sparse_cache *dma_buf = buffer_acquire(dd->dma_buffer),
		*local_buf = buffer_acquire(dd->local_buffer);
	uint32_t local_fmt = local_buf->stream.frame_fmt;
	uint32_t dma_fmt = dma_buf->stream.frame_fmt;
	uint32_t fifo;
	int i, err = 0;

	buffer_release(local_buf);

	/* set processing function */
	dd->process = pcm_get_conversion_function(local_fmt, dma_fmt);

	if (!dd->process) {
		comp_err(dev, "dai_playback_params(): converter function NULL: local fmt %d dma fmt %d\n",
			 local_fmt, dma_fmt);
		err = -EINVAL;
		goto out;
	}

	/* set up DMA configuration */
	config->direction = DMA_DIR_MEM_TO_DEV;
	config->src_width = get_sample_bytes(dma_fmt);
	config->dest_width = config->src_width;
	config->cyclic = 1;
	config->irq_disabled = pipeline_is_timer_driven(dev->pipeline);
	config->dest_dev = dai_get_handshake(dd->dai, dev->direction, dd->stream_id);
	config->is_scheduling_source = comp_is_scheduling_source(dev);
	config->period = dev->pipeline->period;

	comp_info(dev, "dai_playback_params() dest_dev = %d stream_id = %d src_width = %d dest_width = %d",
		  config->dest_dev, dd->stream_id,
		  config->src_width, config->dest_width);

	if (!config->elem_array.elems) {
		fifo = dai_get_fifo(dd->dai, dev->direction,
				    dd->stream_id);

		comp_info(dev, "dai_playback_params() fifo 0x%x", fifo);

		err = dma_sg_alloc(&config->elem_array, SOF_MEM_ZONE_RUNTIME,
				   config->direction,
				   period_count,
				   period_bytes,
				   (uintptr_t)(dma_buf->stream.addr),
				   fifo);
		if (err < 0) {
			comp_err(dev, "dai_playback_params(): dma_sg_alloc() for period_count %d period_bytes %d failed with err = %d",
				 period_count, period_bytes, err);
			goto out;
		}
	}

	dma_cfg = rballoc(SOF_MEM_FLAG_COHERENT,
			  SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA,
			  sizeof(struct dma_config));
	if (!dma_cfg) {
		comp_err(dev, "dai_playback_params(): dma_cfg allocation failed");
		err = -ENOMEM;
		goto out;
	}

	dma_cfg->channel_direction = MEMORY_TO_PERIPHERAL;
	dma_cfg->source_data_size = config->src_width;
	dma_cfg->dest_data_size = config->dest_width;

	if (config->burst_elems)
		dma_cfg->source_burst_length = config->burst_elems;
	else
		dma_cfg->source_burst_length = 8;

	dma_cfg->dest_burst_length = dma_cfg->source_burst_length;
	dma_cfg->cyclic = config->cyclic;
	dma_cfg->user_data = NULL;
	dma_cfg->dma_callback = NULL;
	dma_cfg->block_count = config->elem_array.count;
	dma_cfg->dma_slot = config->dest_dev;

	dma_block_cfg = rballoc(SOF_MEM_FLAG_COHERENT,
				SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA,
				sizeof(struct dma_block_config) * dma_cfg->block_count);
	if (!dma_block_cfg) {
		rfree(dma_cfg);
		comp_err(dev, "dai_playback_params(): dma_block_config allocation failed");
		err = -ENOMEM;
		goto out;
	}

	dma_cfg->head_block = dma_block_cfg;
	for (i = 0; i < dma_cfg->block_count; i++) {
		dma_block_cfg->dest_scatter_en = config->scatter;
		dma_block_cfg->block_size = config->elem_array.elems[i].size;
		dma_block_cfg->source_address = config->elem_array.elems[i].src;
		dma_block_cfg->dest_address = config->elem_array.elems[i].dest;
		prev = dma_block_cfg;
		prev->next_block = ++dma_block_cfg;
	}
	if (prev)
		prev->next_block = dma_cfg->head_block;
	dd->z_config = dma_cfg;

out:
	buffer_release(dma_buf);

	return err;
}

static int dai_capture_params(struct comp_dev *dev, uint32_t period_bytes,
			      uint32_t period_count)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	struct dma_config *dma_cfg;
	struct dma_block_config *dma_block_cfg;
	struct dma_block_config *prev = NULL;
	struct comp_buffer __sparse_cache *dma_buf = buffer_acquire(dd->dma_buffer),
		*local_buf = buffer_acquire(dd->local_buffer);
	uint32_t local_fmt = local_buf->stream.frame_fmt;
	uint32_t dma_fmt = dma_buf->stream.frame_fmt;
	uint32_t fifo;
	int i, err = 0;

	buffer_release(local_buf);

	/* set processing function */
	dd->process = pcm_get_conversion_function(dma_fmt, local_fmt);

	if (!dd->process) {
		comp_err(dev, "dai_capture_params(): converter function NULL: local fmt %d dma fmt %d\n",
			 local_fmt, dma_fmt);
		err = -EINVAL;
		goto out;
	}

	/* set up DMA configuration */
	config->direction = DMA_DIR_DEV_TO_MEM;
	config->cyclic = 1;
	config->irq_disabled = pipeline_is_timer_driven(dev->pipeline);
	config->src_dev = dai_get_handshake(dd->dai, dev->direction,
					    dd->stream_id);
	config->is_scheduling_source = comp_is_scheduling_source(dev);
	config->period = dev->pipeline->period;

	/* TODO: Make this code platform-specific or move it driver callback */
	if (dd->dai->type == SOF_DAI_INTEL_DMIC) {
		/* For DMIC the DMA src and dest widths should always be 4 bytes
		 * due to 32 bit FIFO packer. Setting width to 2 bytes for
		 * 16 bit format would result in recording at double rate.
		 */
		config->src_width = 4;
		config->dest_width = 4;
	} else {
		config->src_width = get_sample_bytes(dma_fmt);
		config->dest_width = config->src_width;
	}

	comp_info(dev, "dai_capture_params() src_dev = %d stream_id = %d src_width = %d dest_width = %d",
		  config->src_dev, dd->stream_id,
		  config->src_width, config->dest_width);

	if (!config->elem_array.elems) {
		fifo = dai_get_fifo(dd->dai, dev->direction,
				    dd->stream_id);

		comp_info(dev, "dai_capture_params() fifo 0x%x", fifo);

		err = dma_sg_alloc(&config->elem_array, SOF_MEM_ZONE_RUNTIME,
				   config->direction,
				   period_count,
				   period_bytes,
				   (uintptr_t)(dma_buf->stream.addr),
				   fifo);
		if (err < 0) {
			comp_err(dev, "dai_capture_params(): dma_sg_alloc() for period_count %d period_bytes %d failed with err = %d",
				 period_count, period_bytes, err);
			goto out;
		}
	}

	dma_cfg = rballoc(SOF_MEM_FLAG_COHERENT,
			  SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA,
			  sizeof(struct dma_config));
	if (!dma_cfg) {
		comp_err(dev, "dai_playback_params(): dma_cfg allocation failed");
		err = -ENOMEM;
		goto out;
	}

	dma_cfg->channel_direction = PERIPHERAL_TO_MEMORY;
	dma_cfg->source_data_size = config->src_width;
	dma_cfg->dest_data_size = config->dest_width;

	if (config->burst_elems)
		dma_cfg->source_burst_length = config->burst_elems;
	else
		dma_cfg->source_burst_length = 8;

	dma_cfg->dest_burst_length = dma_cfg->source_burst_length;
	dma_cfg->cyclic = config->cyclic;
	dma_cfg->user_data = NULL;
	dma_cfg->dma_callback = NULL;
	dma_cfg->block_count = config->elem_array.count;
	dma_cfg->dma_slot = config->src_dev;

	dma_block_cfg = rballoc(SOF_MEM_FLAG_COHERENT,
				SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA,
				sizeof(struct dma_block_config) * dma_cfg->block_count);
	if (!dma_block_cfg) {
		rfree(dma_cfg);
		comp_err(dev, "dai_playback_params(): dma_block_config allocation failed");
		err = -ENOMEM;
		goto out;
	}

	dma_cfg->head_block = dma_block_cfg;
	for (i = 0; i < dma_cfg->block_count; i++) {
		dma_block_cfg->dest_scatter_en = config->scatter;
		dma_block_cfg->block_size = config->elem_array.elems[i].size;
		dma_block_cfg->source_address = config->elem_array.elems[i].src;
		dma_block_cfg->dest_address = config->elem_array.elems[i].dest;
		prev = dma_block_cfg;
		prev->next_block = ++dma_block_cfg;
	}
	if (prev)
		prev->next_block = dma_cfg->head_block;
	dd->z_config = dma_cfg;

out:
	buffer_release(dma_buf);

	return err;
}

static int dai_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	struct sof_ipc_stream_params hw_params = *params;
	struct dai_data *dd = comp_get_drvdata(dev);
	struct comp_buffer __sparse_cache *buffer_c;
	uint32_t frame_size;
	uint32_t period_count;
	uint32_t period_bytes;
	uint32_t buffer_size;
	uint32_t addr_align;
	uint32_t align;
	int err;

	comp_dbg(dev, "dai_params()");

	/* configure dai_data first */
	err = ipc_dai_data_config(dev);
	if (err < 0)
		return err;

	err = dai_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "dai_params(): pcm params verification failed.");
		return -EINVAL;
	}

	/* params verification passed, so now configure hw dai stream params */
	err = dai_comp_hw_params(dev, params);
	if (err < 0) {
		comp_err(dev, "dai_params(): dai_comp_hw_params failed err %d", err);
		return err;
	}

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		dd->local_buffer = list_first_item(&dev->bsource_list,
						   struct comp_buffer,
						   sink_list);
	else
		dd->local_buffer = list_first_item(&dev->bsink_list,
						   struct comp_buffer,
						   source_list);

	/* check if already configured */
	if (dev->state == COMP_STATE_PREPARE) {
		comp_info(dev, "dai_params() component has been already configured.");
		return 0;
	}

	/* can set params on only init state */
	if (dev->state != COMP_STATE_READY) {
		comp_err(dev, "dai_params(): Component is in state %d, expected COMP_STATE_READY.",
			 dev->state);
		return -EINVAL;
	}

	err = dma_get_attribute(dd->dma, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				&addr_align);
	if (err < 0) {
		comp_err(dev, "dai_params(): could not get dma buffer address alignment, err = %d",
			 err);
		return err;
	}

	err = dma_get_attribute(dd->dma, DMA_ATTR_BUFFER_ALIGNMENT, &align);
	if (err < 0 || !align) {
		comp_err(dev, "dai_params(): no valid dma buffer alignment, err = %d, align = %u",
			 err, align);
		return -EINVAL;
	}

	err = dma_get_attribute(dd->dma, DMA_ATTR_BUFFER_PERIOD_COUNT,
				&period_count);
	if (err < 0 || !period_count) {
		comp_err(dev, "dai_params(): no valid dma buffer period count, err = %d, period_count = %u",
			 err, period_count);
		return -EINVAL;
	}

	buffer_c = buffer_acquire(dd->local_buffer);

	/* calculate frame size */
	frame_size = get_frame_bytes(dev->ipc_config.frame_fmt,
				     buffer_c->stream.channels);

	buffer_release(buffer_c);

	/* calculate period size */
	period_bytes = dev->frames * frame_size;
	if (!period_bytes) {
		comp_err(dev, "dai_params(): invalid period_bytes.");
		return -EINVAL;
	}

	dd->period_bytes = period_bytes;

	/* calculate DMA buffer size */
	period_count = MAX(period_count,
			   DIV_ROUND_UP(dd->ipc_config.dma_buffer_size, period_bytes));
	buffer_size = ALIGN_UP(period_count * period_bytes, align);

	/* alloc DMA buffer or change its size if exists */
	if (dd->dma_buffer) {
		buffer_c = buffer_acquire(dd->dma_buffer);
		err = buffer_set_size(buffer_c, buffer_size);
		buffer_release(buffer_c);

		if (err < 0) {
			comp_err(dev, "dai_params(): buffer_set_size() failed, buffer_size = %u",
				 buffer_size);
			return err;
		}
	} else {
		dd->dma_buffer = buffer_alloc(buffer_size, SOF_MEM_CAPS_DMA,
					      addr_align);
		if (!dd->dma_buffer) {
			comp_err(dev, "dai_params(): failed to alloc dma buffer");
			return -ENOMEM;
		}

		/*
		 * dma_buffer should reffer to hardware dai parameters.
		 * Here, we overwrite frame_fmt hardware parameter as DAI
		 * component is able to convert stream with different
		 * frame_fmt's (using pcm converter).
		 */
		hw_params.frame_fmt = dev->ipc_config.frame_fmt;
		buffer_c = buffer_acquire(dd->dma_buffer);
		buffer_set_params(buffer_c, &hw_params,
				  BUFFER_UPDATE_FORCE);
		buffer_release(buffer_c);
	}

	return dev->direction == SOF_IPC_STREAM_PLAYBACK ?
		dai_playback_params(dev, period_bytes, period_count) :
		dai_capture_params(dev, period_bytes, period_count);
}

static int dai_config_prepare(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	int channel;

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_config_prepare(): Component is in active state.");
		return 0;
	}

	if (!dd->dai_spec_config) {
		comp_err(dev, "dai specific config is not set yet!");
		return -EINVAL;
	}

	if (dd->chan) {
		comp_info(dev, "dai_config_prepare(): dma channel index %d already configured",
			  dd->chan->index);
		return 0;
	}

	channel = dai_config_dma_channel(dev, dd->dai_spec_config);
	comp_info(dev, "dai_config_prepare(), channel = %d", channel);

	/* do nothing for asking for channel free, for compatibility. */
	if (channel == DMA_CHAN_INVALID) {
		comp_err(dev, "dai_config is not set yet!");
		return -EINVAL;
	}

	/* get DMA channel */
	channel = dma_request_channel(dd->dma->z_dev, &channel);
	if (channel < 0) {
		comp_err(dev, "dai_config_prepare(): dma_request_channel() failed");
		dd->chan = NULL;
		return -EIO;
	}

	dd->chan = &dd->dma->chan[channel];
	dd->chan->dev_data = dd;

	comp_info(dev, "dai_config_prepare(): new configured dma channel index %d",
		  dd->chan->index);

	/* setup callback */
	notifier_register(dev, dd->chan, NOTIFIER_ID_DMA_COPY,
			  dai_dma_cb, 0);

	return 0;
}

static int dai_prepare(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct comp_buffer __sparse_cache *buffer_c;
	int ret;

	comp_info(dev, "dai_prepare()");

	ret = dai_config_prepare(dev);
	if (ret < 0)
		return ret;

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	dd->total_data_processed = 0;

	if (!dd->chan) {
		comp_err(dev, "dai_prepare(): Missing dd->chan.");
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return -EINVAL;
	}

	if (!dd->config.elem_array.elems) {
		comp_err(dev, "dai_prepare(): Missing dd->config.elem_array.elems.");
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return -EINVAL;
	}

	/* clear dma buffer to avoid pop noise */
	buffer_c = buffer_acquire(dd->dma_buffer);
	buffer_zero(buffer_c);
	buffer_release(buffer_c);

	/* dma reconfig not required if XRUN handling */
	if (dd->xrun) {
		/* after prepare, we have recovered from xrun */
		dd->xrun = 0;
		return ret;
	}

	ret = dma_config(dd->chan->dma->z_dev, dd->chan->index, dd->z_config);
	if (ret < 0)
		comp_set_state(dev, COMP_TRIGGER_RESET);

	return ret;
}

static int dai_reset(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;

	comp_info(dev, "dai_reset()");

	/*
	 * DMA channel release should be skipped now for DAI's that support the two-step stop
	 * option. It will be done when the host sends the DAI_CONFIG IPC during hw_free.
	 */
	if (!dd->delayed_dma_stop)
		dai_dma_release(dev);

	dma_sg_free(&config->elem_array);
	rfree(dd->z_config);

	if (dd->dma_buffer) {
		buffer_free(dd->dma_buffer);
		dd->dma_buffer = NULL;
	}

	dd->wallclock = 0;
	dd->total_data_processed = 0;
	dd->xrun = 0;
	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

static void dai_update_start_position(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	/* update starting wallclock */
	platform_dai_wallclock(dev, &dd->wallclock);
}

/* used to pass standard and bespoke command (with data) to component */
static int dai_comp_trigger_internal(struct comp_dev *dev, int cmd)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	int ret;

	comp_dbg(dev, "dai_comp_trigger_internal(), command = %u", cmd);

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	switch (cmd) {
	case COMP_TRIGGER_START:
		comp_dbg(dev, "dai_comp_trigger_internal(), START");

		/* only start the DAI if we are not XRUN handling */
		if (dd->xrun == 0) {
			ret = dma_start(dd->chan->dma->z_dev, dd->chan->index);
			if (ret < 0)
				return ret;

			/* start the DAI */
			dai_trigger_op(dd->dai, cmd, dev->direction);
		} else {
			dd->xrun = 0;
		}

		dai_update_start_position(dev);
		break;
	case COMP_TRIGGER_RELEASE:
		/* before release, we clear the buffer data to 0s,
		 * then there is no history data sent out after release.
		 * this is only supported at capture mode.
		 */
		if (dev->direction == SOF_IPC_STREAM_CAPTURE) {
			struct comp_buffer __sparse_cache *buffer_c =
				buffer_acquire(dd->dma_buffer);

			buffer_zero(buffer_c);
			buffer_release(buffer_c);
		}

		/* only start the DAI if we are not XRUN handling */
		if (dd->xrun == 0) {
			/* recover valid start position */
			ret = dma_stop(dd->chan->dma->z_dev, dd->chan->index);
			if (ret < 0)
				return ret;

			/* start the DAI */
			dai_trigger_op(dd->dai, cmd, dev->direction);
			ret = dma_start(dd->chan->dma->z_dev, dd->chan->index);
			if (ret < 0)
				return ret;
		} else {
			dd->xrun = 0;
		}

		dai_update_start_position(dev);
		break;
	case COMP_TRIGGER_XRUN:
		comp_info(dev, "dai_comp_trigger_internal(), XRUN");
		dd->xrun = 1;

		COMPILER_FALLTHROUGH;
	case COMP_TRIGGER_STOP:
		comp_dbg(dev, "dai_comp_trigger_internal(), STOP");
/*
 * Some platforms cannot just simple disable
 * DMA channel during the transfer,
 * because it will hang the whole DMA controller.
 * Therefore, stop the DMA first and let the DAI
 * drain the FIFO in order to stop the channel
 * as soon as possible.
 */
#if CONFIG_DMA_SUSPEND_DRAIN
		ret = dma_stop(dd->chan->dma->z_dev, dd->chan->index);
		dai_trigger_op(dd->dai, cmd, dev->direction);
#else
		dai_trigger_op(dd->dai, cmd, dev->direction);
		ret = dma_stop(dd->chan->dma->z_dev, dd->chan->index);
#endif
		break;
	case COMP_TRIGGER_PAUSE:
		comp_dbg(dev, "dai_comp_trigger_internal(), PAUSE");
		ret = dma_suspend(dd->chan->dma->z_dev, dd->chan->index);
		dai_trigger_op(dd->dai, cmd, dev->direction);
		break;
	case COMP_TRIGGER_PRE_START:
	case COMP_TRIGGER_PRE_RELEASE:
		/* only start the DAI if we are not XRUN handling */
		if (dd->xrun)
			dd->xrun = 0;
		else
			dai_trigger_op(dd->dai, cmd, dev->direction);
		break;
	}

	return ret;
}

static int dai_comp_trigger(struct comp_dev *dev, int cmd)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dai_group *group = dd->group;
	uint32_t irq_flags;
	int ret = 0;

	/* DAI not in a group, use normal trigger */
	if (!group) {
		comp_dbg(dev, "dai_comp_trigger(), non-atomic trigger");
		return dai_comp_trigger_internal(dev, cmd);
	}

	/* DAI is grouped, so only trigger when the entire group is ready */

	if (!group->trigger_counter) {
		/* First DAI to receive the trigger command,
		 * prepare for atomic trigger
		 */
		comp_dbg(dev, "dai_comp_trigger(), begin atomic trigger for group %d",
			 group->group_id);
		group->trigger_cmd = cmd;
		group->trigger_counter = group->num_dais - 1;
	} else if (group->trigger_cmd != cmd) {
		/* Already processing a different trigger command */
		comp_err(dev, "dai_comp_trigger(), already processing atomic trigger");
		ret = -EAGAIN;
	} else {
		/* Count down the number of remaining DAIs required
		 * to receive the trigger command before atomic trigger
		 * takes place
		 */
		group->trigger_counter--;
		comp_dbg(dev, "dai_comp_trigger(), trigger counter %d, group %d",
			 group->trigger_counter, group->group_id);

		if (!group->trigger_counter) {
			/* The counter has reached 0, which means
			 * all DAIs have received the same trigger command
			 * and we may begin the actual trigger process
			 * synchronously.
			 */

			irq_local_disable(irq_flags);
			notifier_event(group, NOTIFIER_ID_DAI_TRIGGER,
				       BIT(cpu_get_id()), NULL, 0);
			irq_local_enable(irq_flags);

			/* return error of last trigger */
			ret = group->trigger_ret;
		}
	}

	return ret;
}

/* report xrun occurrence */
static void dai_report_xrun(struct comp_dev *dev, uint32_t bytes)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct comp_buffer __sparse_cache *buf_c = buffer_acquire(dd->local_buffer);

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		comp_err(dev, "dai_report_xrun(): underrun due to no data available");
		comp_underrun(dev, buf_c, bytes);
	} else {
		comp_err(dev, "dai_report_xrun(): overrun due to no space available");
		comp_overrun(dev, buf_c, bytes);
	}

	buffer_release(buf_c);
}

/* copy and process stream data from source to sink buffers */
static int dai_copy(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	uint32_t dma_fmt;
	uint32_t sampling;
	struct comp_buffer __sparse_cache *buf_c;
	struct dma_status stat;
	uint32_t avail_bytes = 0;
	uint32_t free_bytes = 0;
	uint32_t copy_bytes = 0;
	uint32_t src_samples;
	uint32_t sink_samples;
	uint32_t samples;
	int ret;

	comp_dbg(dev, "dai_copy()");

	/* get data sizes from DMA */
	ret = dma_get_status(dd->chan->dma->z_dev, dd->chan->index, &stat);
	if (ret < 0) {
		dai_report_xrun(dev, 0);
		return ret;
	}
	avail_bytes = stat.pending_length;
	free_bytes = stat.free;

	buf_c = buffer_acquire(dd->dma_buffer);

	dma_fmt = buf_c->stream.frame_fmt;
	sampling = get_sample_bytes(dma_fmt);

	buffer_release(buf_c);

	buf_c = buffer_acquire(dd->local_buffer);

	/* calculate minimum size to copy */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		src_samples = audio_stream_get_avail_samples(&buf_c->stream);
		sink_samples = free_bytes / sampling;
		samples = MIN(src_samples, sink_samples);
	} else {
		src_samples = avail_bytes / sampling;
		sink_samples = audio_stream_get_free_samples(&buf_c->stream);
		samples = MIN(src_samples, sink_samples);
	}

	/* limit bytes per copy to one period for the whole pipeline
	 * in order to avoid high load spike
	 */
	samples = MIN(samples, dd->period_bytes / sampling);

	copy_bytes = samples * sampling;

	comp_dbg(dev, "dai_copy(), dir: %d copy_bytes= 0x%x, frames= %d",
		 dev->direction, copy_bytes,
		 samples / buf_c->stream.channels);

	buffer_release(buf_c);

	/* Check possibility of glitch occurrence */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK &&
	    copy_bytes + avail_bytes < dd->period_bytes)
		comp_warn(dev, "dai_copy(): Copy_bytes %d + avail bytes %d < period bytes %d, possible glitch",
			  copy_bytes, avail_bytes, dd->period_bytes);
	else if (dev->direction == SOF_IPC_STREAM_CAPTURE &&
		 copy_bytes + free_bytes < dd->period_bytes)
		comp_warn(dev, "dai_copy(): Copy_bytes %d + free bytes %d < period bytes %d, possible glitch",
			  copy_bytes, free_bytes, dd->period_bytes);

	/* return if nothing to copy */
	if (!copy_bytes) {
		comp_warn(dev, "dai_copy(): nothing to copy");
		return 0;
	}

	/* trigger optional DAI_TRIGGER_COPY which prepares dai to copy */
	ret = dai_trigger(dd->dai->dev, dev->direction, DAI_TRIGGER_COPY);
	if (ret < 0)
		comp_warn(dev, "dai_copy(): dai trigger copy failed");

	struct dma_cb_data next = {
		.channel = dd->chan,
		.elem = { .size = copy_bytes },
		.status = DMA_CB_STATUS_END,
	};

	notifier_event(dd->chan, NOTIFIER_ID_DMA_COPY,
		       NOTIFIER_TARGET_CORE_LOCAL, &next, sizeof(next));

	if (next.status == DMA_CB_STATUS_END)
		dma_stop(dd->chan->dma->z_dev, dd->chan->index);

	ret = dma_reload(dd->chan->dma->z_dev, dd->chan->index, 0, 0, copy_bytes);
	if (ret < 0) {
		dai_report_xrun(dev, copy_bytes);
		return ret;
	}

	dai_dma_position_update(dev);

	return ret;
}

/**
 * \brief Get DAI parameters and configure timestamping
 * \param[in, out] dev DAI device.
 * \return Error code.
 *
 * This function retrieves various DAI parameters such as type, direction, index, and DMA
 * controller information those are needed when configuring HW timestamping. Note that
 * DAI must be prepared before this function is used (for DMA information). If not, an error
 * is returned.
 */
static int dai_ts_config_op(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct ipc_config_dai *dai = &dd->ipc_config;
	struct dai_ts_cfg cfg;

	comp_dbg(dev, "dai_ts_config()");
	if (!dd->chan) {
		comp_err(dev, "dai_ts_config(), No DMA channel information");
		return -EINVAL;
	}

	switch (dai->type) {
	case SOF_DAI_INTEL_SSP:
		cfg.type = DAI_INTEL_SSP;
		break;
	case SOF_DAI_INTEL_ALH:
		cfg.type = DAI_INTEL_ALH;
		break;
	case SOF_DAI_INTEL_DMIC:
		cfg.type = DAI_INTEL_DMIC;
		break;
	default:
		comp_err(dev, "dai_ts_config(), not supported dai type");
		return -EINVAL;
	}

	cfg.direction = dai->direction;
	cfg.index = dd->dai->index;
	cfg.dma_id = dd->dma->plat_data.id;
	cfg.dma_chan_index = dd->chan->index;
	cfg.dma_chan_count = dd->dma->plat_data.channels;

	return dai_ts_config(dd->dai->dev, &cfg);
}

static int dai_ts_start_op(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dai_ts_cfg cfg;

	comp_dbg(dev, "dai_ts_start()");

	return dai_ts_start(dd->dai->dev, &cfg);
}

static int dai_ts_get_op(struct comp_dev *dev, struct timestamp_data *tsd)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dai_ts_data tsdata;
	struct dai_ts_cfg cfg;
	int ret;

	comp_dbg(dev, "dai_ts_get()");

	ret = dai_ts_get(dd->dai->dev, &cfg, &tsdata);

	if (ret < 0)
		return ret;

	/* todo convert to timestamp_data */

	return ret;
}

static int dai_ts_stop_op(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dai_ts_cfg cfg;

	comp_dbg(dev, "dai_ts_stop()");

	return dai_ts_stop(dd->dai->dev, &cfg);
}

uint32_t dai_get_init_delay_ms(struct dai *dai)
{
	const struct dai_properties *props = dai_get_properties(dai->dev, 0, 0);

	return props->reg_init_delay;
}

static uint64_t dai_get_processed_data(struct comp_dev *dev, uint32_t stream_no, bool input)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	uint64_t ret = 0;
	bool source = dev->direction == SOF_IPC_STREAM_CAPTURE;

	/* Return value only if direction and stream number match.
	 * The dai supports only one stream.
	 */
	if (stream_no == 0 && source == input)
		ret = dd->total_data_processed;

	return ret;
}

static const struct comp_driver comp_dai = {
	.type	= SOF_COMP_DAI,
	.uid	= SOF_RT_UUID(dai_comp_uuid),
	.tctx	= &dai_comp_tr,
	.ops	= {
		.create				= dai_new,
		.free				= dai_free,
		.params				= dai_params,
		.dai_get_hw_params		= dai_comp_get_hw_params,
		.trigger			= dai_comp_trigger,
		.copy				= dai_copy,
		.prepare			= dai_prepare,
		.reset				= dai_reset,
		.position			= dai_position,
		.dai_config			= dai_config,
		.dai_ts_config			= dai_ts_config_op,
		.dai_ts_start			= dai_ts_start_op,
		.dai_ts_stop			= dai_ts_stop_op,
		.dai_ts_get			= dai_ts_get_op,
		.get_total_data_processed	= dai_get_processed_data,
},
};

static SHARED_DATA struct comp_driver_info comp_dai_info = {
	.drv = &comp_dai,
};

UT_STATIC void sys_comp_dai_init(void)
{
	comp_register(platform_shared_get(&comp_dai_info, sizeof(comp_dai_info)));
}

DECLARE_MODULE(sys_comp_dai_init);
