// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/pcm_converter.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/edma.h>
#include <sof/drivers/interrupt.h>
#include <sof/ipc/msg.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/dai.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static const struct comp_driver comp_dai;

/* c2b00d27-ffbc-4150-a51a-245c79c5e54b */
DECLARE_SOF_RT_UUID("dai", dai_comp_uuid, 0xc2b00d27, 0xffbc, 0x4150,
		 0xa5, 0x1a, 0x24, 0x5c, 0x79, 0xc5, 0xe5, 0x4b);

DECLARE_TR_CTX(dai_comp_tr, SOF_UUID(dai_comp_uuid), LOG_LEVEL_INFO);

struct dai_data {
	/* local DMA config */
	struct dma_chan_data *chan;
	uint32_t stream_id;
	struct dma_sg_config config;
	struct comp_buffer *dma_buffer;
	struct comp_buffer *local_buffer;
	struct timestamp_cfg ts_config;
	struct dai *dai;
	struct dma *dma;
	struct dai_group *group;	/**< NULL if no group assigned */
	int xrun;		/* true if we are doing xrun recovery */

	pcm_converter_func process;	/* processing function */

	uint32_t dai_pos_blks;	/* position in bytes (nearest block) */
	uint64_t start_position;	/* position on start */
	uint32_t period_bytes;	/**< number of bytes per one period */

	/* host can read back this value without IPC */
	uint64_t *dai_pos;

	struct sof_ipc_dai_config *dai_config;	/* dai_config from the host */

	uint64_t wallclock;	/* wall clock at stream start */
};

static void dai_atomic_trigger(void *arg, enum notify_id type, void *data);

/* Assign DAI to a group */
static int dai_assign_group(struct comp_dev *dev, uint32_t group_id)
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

/* this is called by DMA driver every time descriptor has completed */
static void dai_dma_cb(void *arg, enum notify_id type, void *data)
{
	struct dma_cb_data *next = data;
	struct comp_dev *dev = arg;
	struct dai_data *dd = comp_get_drvdata(dev);
	uint32_t bytes = next->elem.size;
	struct comp_buffer *source;
	struct comp_buffer *sink;
	void *buffer_ptr;
	int ret;

	comp_dbg(dev, "dai_dma_cb()");

	next->status = DMA_CB_STATUS_RELOAD;

	/* stop dma copy for pause/stop/xrun */
	if (dev->state != COMP_STATE_ACTIVE || dd->xrun) {
		/* stop the DAI */
		dai_trigger(dd->dai, COMP_TRIGGER_STOP, dev->direction);

		/* tell DMA not to reload */
		next->status = DMA_CB_STATUS_END;
	}

	/* is our pipeline handling an XRUN ? */
	if (dd->xrun) {
		/* make sure we only playback silence during an XRUN */
		if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
			/* fill buffer with silence */
			buffer_zero(dd->dma_buffer);

		return;
	}

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		ret = dma_buffer_copy_to(dd->local_buffer, dd->dma_buffer,
					 dd->process, bytes);

		buffer_ptr = dd->local_buffer->stream.r_ptr;
	} else {
		ret = dma_buffer_copy_from(dd->dma_buffer, dd->local_buffer,
					   dd->process, bytes);

		buffer_ptr = dd->local_buffer->stream.w_ptr;
	}

	/* assert dma_buffer_copy succeed */
	if (ret < 0) {
		source = dev->direction == SOF_IPC_STREAM_PLAYBACK ?
					dd->local_buffer : dd->dma_buffer;
		sink = dev->direction == SOF_IPC_STREAM_PLAYBACK ?
					dd->dma_buffer : dd->local_buffer;
		comp_err(dev, "dai_dma_cb() dma buffer copy failed, dir %d bytes %d avail %d free %d",
			 dev->direction, bytes,
			 audio_stream_get_avail_samples(&source->stream) *
				audio_stream_frame_bytes(&source->stream),
			 audio_stream_get_free_samples(&sink->stream) *
				audio_stream_frame_bytes(&sink->stream));
		return;
	}

	/* update host position (in bytes offset) for drivers */
	dev->position += bytes;
	if (dd->dai_pos) {
		dd->dai_pos_blks += bytes;
		*dd->dai_pos = dd->dai_pos_blks +
			       (char *)buffer_ptr -
			       (char *)dd->dma_buffer->stream.addr;
	}
}

static struct comp_dev *dai_new(const struct comp_driver *drv,
				struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_dai *dai;
	struct sof_ipc_comp_dai *ipc_dai = (struct sof_ipc_comp_dai *)comp;
	struct dai_data *dd;
	uint32_t dir, caps, dma_dev;
	int ret;

	comp_cl_dbg(&comp_dai, "dai_new()");

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_dai));
	if (!dev)
		return NULL;

	dai = COMP_GET_IPC(dev, sof_ipc_comp_dai);
	ret = memcpy_s(dai, sizeof(*dai), ipc_dai,
		       sizeof(struct sof_ipc_comp_dai));
	assert(!ret);

	dd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*dd));
	if (!dd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, dd);

	dd->dai = dai_get(dai->type, dai->dai_index, DAI_CREAT);
	if (!dd->dai) {
		comp_cl_err(&comp_dai, "dai_new(): dai_get() failed to create DAI.");
		goto error;
	}

	/* request GP LP DMA with shared access privilege */
	dir = dai->direction == SOF_IPC_STREAM_PLAYBACK ?
			DMA_DIR_MEM_TO_DEV : DMA_DIR_DEV_TO_MEM;

	caps = dai_get_info(dd->dai, DAI_INFO_DMA_CAPS);
	dma_dev = dai_get_info(dd->dai, DAI_INFO_DMA_DEV);

	dd->dma = dma_get(dir, caps, dma_dev, DMA_ACCESS_SHARED);
	if (!dd->dma) {
		comp_cl_err(&comp_dai, "dai_new(): dma_get() failed to get shared access to DMA.");
		goto error;
	}

	dma_sg_init(&dd->config.elem_array);
	dd->dai_pos = NULL;
	dd->dai_pos_blks = 0;
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
		dma_channel_put(dd->chan);
	}

	dma_put(dd->dma);

	dai_put(dd->dai);

	if (dd->dai_config)
		rfree(dd->dai_config);

	rfree(dd);
	rfree(dev);
}

static int dai_comp_get_hw_params(struct comp_dev *dev,
				  struct sof_ipc_stream_params *params,
				  int dir)
{
	struct sof_ipc_comp_config *dconfig = dev_comp_config(dev);
	struct dai_data *dd = comp_get_drvdata(dev);
	int ret = 0;

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
	params->frame_fmt = dconfig->frame_fmt;

	return 0;
}

static int dai_comp_hw_params(struct comp_dev *dev,
			      struct sof_ipc_stream_params *params)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	int ret;

	comp_dbg(dev, "dai_comp_hw_params()");

	/* configure hw dai stream params */
	ret = dai_hw_params(dd->dai, params);
	if (ret < 0) {
		comp_err(dev, "dai_comp_hw_params(): dai_hw_params failed ret %d",
			 ret);
		return ret;
	}

	return 0;
}

static int dai_verify_params(struct comp_dev *dev,
			     struct sof_ipc_stream_params *params)
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
	component_set_period_frames(dev, params->rate);

	return 0;
}

static void dai_data_config(struct comp_dev *dev)
{
	struct sof_ipc_comp_config *dconfig = dev_comp_config(dev);
	struct dai_data *dd = comp_get_drvdata(dev);
	struct sof_ipc_comp_dai *dai = COMP_GET_IPC(dev, sof_ipc_comp_dai);

	assert(dd->dai_config);

	comp_info(dev, "dai_data_config() dai type = %d index = %d dd %p",
		  dai->type, dai->dai_index, dd);

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_data_config(): Component is in active state.");
		return;
	}

	switch (dd->dai_config->type) {
	case SOF_DAI_INTEL_SSP:
		/* set dma burst elems to slot number */
		dd->config.burst_elems = dd->dai_config->ssp.tdm_slots;
		break;
	case SOF_DAI_INTEL_DMIC:
		/* We can use always the largest burst length. */
		dd->config.burst_elems = 8;

		comp_info(dev, "config->dmic.fifo_bits = %u config->dmic.num_pdm_active = %u",
			  dd->dai_config->dmic.fifo_bits,
			  dd->dai_config->dmic.num_pdm_active);
		break;
	case SOF_DAI_INTEL_HDA:
		break;
	case SOF_DAI_INTEL_ALH:
		/* SDW HW FIFO always requires 32bit MSB aligned sample data for
		 * all formats, such as 8/16/24/32 bits.
		 */
		dconfig->frame_fmt = SOF_IPC_FRAME_S32_LE;
		dd->dma_buffer->stream.frame_fmt = dconfig->frame_fmt;

		dd->config.burst_elems =
			dd->dai->plat_data.fifo[dai->direction].depth;

		/* As with HDA, the DMA channel is assigned in runtime,
		 * not during topology parsing.
		 */
		dd->stream_id = dd->dai_config->alh.stream_id;
		break;
	case SOF_DAI_IMX_SAI:
		COMPILER_FALLTHROUGH;
	case SOF_DAI_IMX_ESAI:
		dd->config.burst_elems =
			dd->dai->plat_data.fifo[dai->direction].depth;
		break;
	default:
		/* other types of DAIs not handled for now */
		comp_warn(dev, "dai_data_config(): Unknown dai type %d",
			  dd->dai_config->type);
		break;
	}
}

/* set component audio SSP and DMA configuration */
static int dai_playback_params(struct comp_dev *dev, uint32_t period_bytes,
			       uint32_t period_count)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	uint32_t local_fmt = dd->local_buffer->stream.frame_fmt;
	uint32_t dma_fmt = dd->dma_buffer->stream.frame_fmt;
	uint32_t fifo;
	int err;

	/* set processing function */
	dd->process = pcm_get_conversion_function(local_fmt, dma_fmt);

	if (!dd->process) {
		comp_err(dev, "dai_playback_params(): converter function NULL: local fmt %d dma fmt %d\n",
			 local_fmt, dma_fmt);
		return -EINVAL;
	}

	/* set up DMA configuration */
	config->direction = DMA_DIR_MEM_TO_DEV;
	config->src_width = get_sample_bytes(dma_fmt);
	config->dest_width = config->src_width;
	config->cyclic = 1;
	config->irq_disabled = pipeline_is_timer_driven(dev->pipeline);
	config->dest_dev = dai_get_handshake(dd->dai, dev->direction,
					     dd->stream_id);
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
				   (uintptr_t)(dd->dma_buffer->stream.addr),
				   fifo);
		if (err < 0) {
			comp_err(dev, "dai_playback_params(): dma_sg_alloc() for period_count %d period_bytes %d failed with err = %d",
				 period_count, period_bytes, err);
			return err;
		}
	}

	return 0;
}

static int dai_capture_params(struct comp_dev *dev, uint32_t period_bytes,
			      uint32_t period_count)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	uint32_t local_fmt = dd->local_buffer->stream.frame_fmt;
	uint32_t dma_fmt = dd->dma_buffer->stream.frame_fmt;
	uint32_t fifo;
	int err;

	/* set processing function */
	dd->process = pcm_get_conversion_function(dma_fmt, local_fmt);

	if (!dd->process) {
		comp_err(dev, "dai_capture_params(): converter function NULL: local fmt %d dma fmt %d\n",
			 local_fmt, dma_fmt);
		return -EINVAL;
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
	if (dai_get_info(dd->dai, DAI_INFO_TYPE) == SOF_DAI_INTEL_DMIC) {
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
				   (uintptr_t)(dd->dma_buffer->stream.addr),
				   fifo);
		if (err < 0) {
			comp_err(dev, "dai_capture_params(): dma_sg_alloc() for period_count %d period_bytes %d failed with err = %d",
				 period_count, period_bytes, err);
			return err;
		}
	}

	return 0;
}

static int dai_params(struct comp_dev *dev,
		      struct sof_ipc_stream_params *params)
{
	struct sof_ipc_comp_config *dconfig = dev_comp_config(dev);
	struct sof_ipc_stream_params hw_params = *params;
	struct dai_data *dd = comp_get_drvdata(dev);
	uint32_t frame_size;
	uint32_t period_count;
	uint32_t period_bytes;
	uint32_t buffer_size;
	uint32_t addr_align;
	uint32_t align;
	int err;

	comp_dbg(dev, "dai_params()");

	/* configure dai_data first */
	dai_data_config(dev);

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
		comp_err(dev, "dai_params(): could not get valid dma buffer alignment, err = %d, align = %u",
			 err, align);
		return -EINVAL;
	}

	err = dma_get_attribute(dd->dma, DMA_ATTR_BUFFER_PERIOD_COUNT,
				&period_count);
	if (err < 0 || !period_count) {
		comp_err(dev, "dai_params(): could not get valid dma buffer period count, err = %d, period_count = %u",
			 err, period_count);
		return -EINVAL;
	}

	/* calculate frame size */
	frame_size = get_frame_bytes(dconfig->frame_fmt,
				     dd->local_buffer->stream.channels);

	/* calculate period size */
	period_bytes = dev->frames * frame_size;
	if (!period_bytes) {
		comp_err(dev, "dai_params(): invalid period_bytes.");
		return -EINVAL;
	}

	dd->period_bytes = period_bytes;

	/* calculate DMA buffer size */
	buffer_size = ALIGN_UP(period_count * period_bytes, align);

	/* alloc DMA buffer or change its size if exists */
	if (dd->dma_buffer) {
		err = buffer_set_size(dd->dma_buffer, buffer_size);
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
		hw_params.frame_fmt = dconfig->frame_fmt;
		buffer_set_params(dd->dma_buffer, &hw_params,
				  BUFFER_UPDATE_FORCE);
	}

	return dev->direction == SOF_IPC_STREAM_PLAYBACK ?
		dai_playback_params(dev, period_bytes, period_count) :
		dai_capture_params(dev, period_bytes, period_count);
}

static int dai_config_dma_channel(struct comp_dev *dev, struct sof_ipc_dai_config *config)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct sof_ipc_comp_dai *dai = COMP_GET_IPC(dev, sof_ipc_comp_dai);
	int channel;
	int handshake;

	assert(config);

	switch (config->type) {
	case SOF_DAI_INTEL_SSP:
		COMPILER_FALLTHROUGH;
	case SOF_DAI_INTEL_DMIC:
		channel = 0;
		break;
	case SOF_DAI_INTEL_HDA:
		channel = config->hda.link_dma_ch;
		break;
	case SOF_DAI_INTEL_ALH:
		/* As with HDA, the DMA channel is assigned in runtime,
		 * not during topology parsing.
		 */
		channel = config->alh.stream_id;
		break;
	case SOF_DAI_IMX_SAI:
		COMPILER_FALLTHROUGH;
	case SOF_DAI_IMX_ESAI:
		handshake = dai_get_handshake(dd->dai, dai->direction,
					      dd->stream_id);
		channel = EDMA_HS_GET_CHAN(handshake);
		break;
	default:
		/* other types of DAIs not handled for now */
		comp_err(dev, "dai_config_dma_channel(): Unknown dai type %d",
			 config->type);
		channel = DMA_CHAN_INVALID;
		break;
	}

	return channel;
}

static int dai_config(struct comp_dev *dev, struct sof_ipc_dai_config *config)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	int ret;

	comp_info(dev, "dai_config() dai type = %d index = %d dd %p",
		  config->type, config->dai_index, dd);

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_config(): Component is in active state. Ignore config");
		return 0;
	}

	if (dd->chan) {
		comp_info(dev, "dai_config(): Configured. dma channel index %d, ignore...",
			  dd->chan->index);
		return 0;
	}

	if (config->group_id) {
		ret = dai_assign_group(dev, config->group_id);

		if (ret)
			return ret;
	}

	/* do nothing for asking for channel free, for compatibility. */
	if (dai_config_dma_channel(dev, config) == DMA_CHAN_INVALID)
		return 0;

	/* allocated dai_config if not yet */
	if (!dd->dai_config) {
		dd->dai_config = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
					 sizeof(*dd->dai_config));
		if (!dd->dai_config) {
			comp_err(dev, "dai_config(): No memory for dai_config.");
			return -ENOMEM;
		}
	}

	*dd->dai_config = *config;

	return 0;
}

static int dai_config_prepare(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	int channel = 0;

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_config_prepare(): Component is in active state.");
		return 0;
	}

	if (!dd->dai_config) {
		comp_err(dev, "dai_config is not set yet!");
		return -EINVAL;
	}

	if (dd->chan) {
		comp_info(dev, "dai_config_prepare(): dma channel index %d already configured",
			  dd->chan->index);
		return 0;
	}

	channel = dai_config_dma_channel(dev, dd->dai_config);
	comp_info(dev, "dai_config_prepare(), channel = %d", channel);

	/* do nothing for asking for channel free, for compatibility. */
	if (channel == DMA_CHAN_INVALID) {
		comp_err(dev, "dai_config is not set yet!");
		return -EINVAL;
	}

	/* allocate DMA channel */
	dd->chan = dma_channel_get(dd->dma, channel);
	if (!dd->chan) {
		comp_err(dev, "dai_config(): dma_channel_get() failed");
		dd->chan = NULL;
		return -EIO;
	}

	comp_info(dev, "dai_config(): new configured dma channel index %d",
		  dd->chan->index);

	/* setup callback */
	notifier_register(dev, dd->chan, NOTIFIER_ID_DMA_COPY,
			  dai_dma_cb, 0);

	return 0;
}

static void dai_config_reset(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_config(): Component is in active state. Ignore resetting");
		return;
	}

	/* put the allocated DMA channel first */
	if (dd->chan) {
		dma_channel_put(dd->chan);
		dd->chan = NULL;

		/* remove callback */
		notifier_unregister(dev, dd->chan,
				    NOTIFIER_ID_DMA_COPY);
	}
}

static int dai_prepare(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	int ret = 0;

	comp_info(dev, "dai_prepare()");

	ret = dai_config_prepare(dev);
	if (ret < 0)
		return ret;

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	dev->position = 0;

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
	buffer_zero(dd->dma_buffer);

	/* dma reconfig not required if XRUN handling */
	if (dd->xrun) {
		/* after prepare, we have recovered from xrun */
		dd->xrun = 0;
		return ret;
	}

	ret = dma_set_config(dd->chan, &dd->config);
	if (ret < 0)
		comp_set_state(dev, COMP_TRIGGER_RESET);

	return ret;
}

static int dai_reset(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;

	comp_info(dev, "dai_reset()");

	dai_config_reset(dev);

	dma_sg_free(&config->elem_array);

	if (dd->dma_buffer) {
		buffer_free(dd->dma_buffer);
		dd->dma_buffer = NULL;
	}

	dd->dai_pos_blks = 0;
	if (dd->dai_pos)
		*dd->dai_pos = 0;
	dd->dai_pos = NULL;
	dd->wallclock = 0;
	dev->position = 0;
	dd->xrun = 0;
	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

static void dai_update_start_position(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	/* update starting wallclock */
	platform_dai_wallclock(dev, &dd->wallclock);

	/* update start position */
	dd->start_position = dev->position;
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
			ret = dma_start(dd->chan);
			if (ret < 0)
				return ret;
			/* start the DAI */
			dai_trigger(dd->dai, cmd, dev->direction);
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
		if (dev->direction == SOF_IPC_STREAM_CAPTURE)
			buffer_zero(dd->dma_buffer);

		/* only start the DAI if we are not XRUN handling */
		if (dd->xrun == 0) {
			/* recover valid start position */
			ret = dma_release(dd->chan);
			if (ret < 0)
				return ret;

			/* start the DAI */
			dai_trigger(dd->dai, cmd, dev->direction);
			ret = dma_start(dd->chan);
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
		ret = dma_stop(dd->chan);
		dai_trigger(dd->dai, cmd, dev->direction);
#else
		dai_trigger(dd->dai, cmd, dev->direction);
		ret = dma_stop(dd->chan);
#endif
		break;
	case COMP_TRIGGER_PAUSE:
		comp_dbg(dev, "dai_comp_trigger_internal(), PAUSE");
		ret = dma_pause(dd->chan);
		dai_trigger(dd->dai, cmd, dev->direction);
		break;
	default:
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

static void dai_atomic_trigger(void *arg, enum notify_id type, void *data)
{
	struct comp_dev *dev = arg;
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dai_group *group = dd->group;

	/* Atomic context set by the last DAI to receive trigger command */
	group->trigger_ret = dai_comp_trigger_internal(dev, group->trigger_cmd);
}

/* report xrun occurrence */
static void dai_report_xrun(struct comp_dev *dev, uint32_t bytes)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		comp_err(dev, "dai_report_xrun(): underrun due to no data available");
		comp_underrun(dev, dd->local_buffer, bytes);
	} else {
		comp_err(dev, "dai_report_xrun(): overrun due to no space available");
		comp_overrun(dev, dd->local_buffer, bytes);
	}
}

/* copy and process stream data from source to sink buffers */
static int dai_copy(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	uint32_t dma_fmt = dd->dma_buffer->stream.frame_fmt;
	const uint32_t sampling = get_sample_bytes(dma_fmt);
	struct comp_buffer *buf = dd->local_buffer;
	uint32_t avail_bytes = 0;
	uint32_t free_bytes = 0;
	uint32_t copy_bytes = 0;
	uint32_t src_samples;
	uint32_t sink_samples;
	uint32_t samples;
	int ret = 0;
	uint32_t flags = 0;

	comp_dbg(dev, "dai_copy()");

	/* get data sizes from DMA */
	ret = dma_get_data_size(dd->chan, &avail_bytes, &free_bytes);
	if (ret < 0) {
		dai_report_xrun(dev, 0);
		return ret;
	}

	buffer_lock(buf, &flags);

	/* calculate minimum size to copy */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		src_samples = audio_stream_get_avail_samples(&buf->stream);
		sink_samples = free_bytes / sampling;
		samples = MIN(src_samples, sink_samples);
	} else {
		src_samples = avail_bytes / sampling;
		sink_samples = audio_stream_get_free_samples(&buf->stream);
		samples = MIN(src_samples, sink_samples);

		/* limit bytes per copy to one period for the whole pipeline
		 * in order to avoid high load spike
		 */
		samples = MIN(samples, dd->period_bytes / sampling);
	}
	copy_bytes = samples * sampling;

	buffer_unlock(buf, flags);

	comp_dbg(dev, "dai_copy(), dir: %d copy_bytes= 0x%x, frames= %d",
		 dev->direction, copy_bytes,
		 samples / buf->stream.channels);

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

	ret = dma_copy(dd->chan, copy_bytes, 0);
	if (ret < 0) {
		dai_report_xrun(dev, copy_bytes);
		return ret;
	}

	return ret;
}

static int dai_position(struct comp_dev *dev, struct sof_ipc_stream_posn *posn)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	/* TODO: improve accuracy by adding current DMA position */
	posn->dai_posn = dev->position;

	/* set stream start wallclock */
	posn->wallclock = dd->wallclock;

	return 0;
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
static int dai_ts_config(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct timestamp_cfg *cfg = &dd->ts_config;
	struct sof_ipc_comp_dai *dai = COMP_GET_IPC(dev, sof_ipc_comp_dai);

	comp_dbg(dev, "dai_ts_config()");
	if (!dd->chan) {
		comp_err(dev, "dai_ts_config(), No DMA channel information");
		return -EINVAL;
	}

	cfg->type = dd->dai->drv->type;
	cfg->direction = dai->direction;
	cfg->index = dd->dai->index;
	cfg->dma_id = dd->dma->plat_data.id;
	cfg->dma_chan_index = dd->chan->index;
	cfg->dma_chan_count = dd->dma->plat_data.channels;
	if (!dd->dai->drv->ts_ops.ts_config)
		return -ENXIO;

	return dd->dai->drv->ts_ops.ts_config(dd->dai, cfg);
}

static int dai_ts_start(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	comp_dbg(dev, "dai_ts_start()");
	if (!dd->dai->drv->ts_ops.ts_start)
		return -ENXIO;

	return dd->dai->drv->ts_ops.ts_start(dd->dai, &dd->ts_config);
}

static int dai_ts_stop(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	comp_dbg(dev, "dai_ts_stop()");
	if (!dd->dai->drv->ts_ops.ts_stop)
		return -ENXIO;

	return dd->dai->drv->ts_ops.ts_stop(dd->dai, &dd->ts_config);
}

static int dai_ts_get(struct comp_dev *dev, struct timestamp_data *tsd)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	comp_dbg(dev, "dai_ts_get()");
	if (!dd->dai->drv->ts_ops.ts_get)
		return -ENXIO;

	return dd->dai->drv->ts_ops.ts_get(dd->dai, &dd->ts_config, tsd);
}

static const struct comp_driver comp_dai = {
	.type	= SOF_COMP_DAI,
	.uid	= SOF_RT_UUID(dai_comp_uuid),
	.tctx	= &dai_comp_tr,
	.ops	= {
		.create			= dai_new,
		.free			= dai_free,
		.params			= dai_params,
		.dai_get_hw_params	= dai_comp_get_hw_params,
		.trigger		= dai_comp_trigger,
		.copy			= dai_copy,
		.prepare		= dai_prepare,
		.reset			= dai_reset,
		.dai_config		= dai_config,
		.position		= dai_position,
		.dai_ts_config		= dai_ts_config,
		.dai_ts_start		= dai_ts_start,
		.dai_ts_stop		= dai_ts_stop,
		.dai_ts_get		= dai_ts_get,
	},
};

static SHARED_DATA struct comp_driver_info comp_dai_info = {
	.drv = &comp_dai,
};

UT_STATIC void sys_comp_dai_init(void)
{
	comp_register(platform_shared_get(&comp_dai_info,
					  sizeof(comp_dai_info)));
}

DECLARE_MODULE(sys_comp_dai_init);
