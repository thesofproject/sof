/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#include <zephyr/drivers/uaol.h>
#include <rtos/string.h>
#include <sof/tlv.h>
#include <sof/audio/component_ext.h>
#include <sof/lib/dai-zephyr.h>
#include <sof/audio/uaol.h>

LOG_MODULE_REGISTER(uaol, CONFIG_SOF_LOG_LEVEL);

struct ipc4_uaol_link_capabilities {
	uint32_t input_streams_supported          : 4;
	uint32_t output_streams_supported         : 4;
	uint32_t bidirectional_streams_supported  : 5;
	uint32_t rsvd                             : 19;
	uint32_t max_tx_fifo_size;
	uint32_t max_rx_fifo_size;
} __packed __aligned(4);

struct ipc4_uaol_capabilities {
	uint32_t link_count;
	struct ipc4_uaol_link_capabilities link_caps[];
} __packed __aligned(4);

#define DEV_AND_COMMA(node) DEVICE_DT_GET(node),
static const struct device *uaol_devs[] = {
	DT_FOREACH_STATUS_OKAY(intel_adsp_uaol, DEV_AND_COMMA)
};

#if !CONFIG_SOF_OS_LINUX_COMPAT_PRIORITY
__cold void tlv_value_set_uaol_caps(struct sof_tlv *tuple, uint32_t type)
{
	const size_t dev_count = ARRAY_SIZE(uaol_devs);
	struct uaol_capabilities dev_cap;
	struct ipc4_uaol_capabilities *caps = (struct ipc4_uaol_capabilities *)tuple->value;
	size_t caps_size = offsetof(struct ipc4_uaol_capabilities, link_caps[dev_count]);
	size_t i;
	int ret;

	assert_can_be_cold();

	memset(caps, 0, caps_size);

	caps->link_count = dev_count;
	for (i = 0; i < dev_count; i++) {
		ret = uaol_get_capabilities(uaol_devs[i], &dev_cap);
		if (ret)
			continue;

		caps->link_caps[i].input_streams_supported = dev_cap.input_streams;
		caps->link_caps[i].output_streams_supported = dev_cap.output_streams;
		caps->link_caps[i].bidirectional_streams_supported = dev_cap.bidirectional_streams;
		caps->link_caps[i].max_tx_fifo_size = dev_cap.max_tx_fifo_size;
		caps->link_caps[i].max_rx_fifo_size = dev_cap.max_rx_fifo_size;
	}

	tlv_value_set(tuple, type, caps_size, caps);
}
#endif /* !CONFIG_SOF_OS_LINUX_COMPAT_PRIORITY */

__cold int uaol_stream_id_to_hda_link_stream_id(int uaol_stream_id)
{
	size_t dev_count = ARRAY_SIZE(uaol_devs);
	size_t i;

	assert_can_be_cold();

	for (i = 0; i < dev_count; i++) {
		int hda_link_stream_id = uaol_get_mapped_hda_link_stream_id(uaol_devs[i],
									    uaol_stream_id);
		if (hda_link_stream_id >= 0)
			return hda_link_stream_id;
	}

	return -1;
}

const struct device *get_uaol_zdevice(int uaol_link_id)
{
	/* uaol_link_id is just an index for the device tree device */
	assert(uaol_link_id < ARRAY_SIZE(uaol_devs));
	return uaol_devs[uaol_link_id];
}

/* These are called from dai-zephyr */

/* The UAOL DAI device has a DMA stream with a corresponding mapped (hardwired in HW)
 * UAOL device stream. These streams are separate entities. This function returns
 * the UAOL device stream ID that is mapped to the UAOL DAI DMA stream.
 */
int dai_get_uaol_stream_id(struct dai *dai, int *uaol_link_id, int *uaol_stream_id)
{
	const struct dai_properties *props;
	k_spinlock_key_t key;

	key = k_spin_lock(&dai->lock);

	props = dai_get_properties(dai->dev, 0, 0);
	*uaol_link_id = props->uaol_link_id;
	*uaol_stream_id = props->uaol_stream_id;

	k_spin_unlock(&dai->lock, key);

	return 0;
}

/* Reads the feedback frequency (USB playback device "desired" frequency) from the feedback
 * USB endpoint via the feedback DMA channel. Then updates the DSRC rate if necessary. Called
 * from the dai-zephyr DMA callback.
 */
void process_uaol_feedback(struct comp_dev *dev, struct dai_data *dd)
{
	assert(dd && dd->uaol.fb_chan_idx >= 0 && dd->uaol.fb_dma_buf);

	struct dma_status stat = {0};
	int ret = sof_dma_get_status(dd->dma, dd->uaol.fb_chan_idx, &stat);
	if (ret) {
		comp_err(dev, "Failed to get UAOL feedback DMA status: %d", ret);
		return;
	}

	/* Expected feedback length is 4 bytes */
	if (stat.pending_length < 4) {
		/* No feedback is normal, as it is sent rarely (e.g., every 128 ms or less often). */
		/* comp_dbg(dev, "No feedback data: %d bytes", stat.pending_length); */
		return;
	}

	assert(is_uncached(dd->uaol.fb_dma_buf));	/* Use uncached pointer to read DMA buffer */
	assert(dd->uaol.fb_dma_buf_size >= 4);

	/* NOTE: Not all DMA drivers populate stat.write_position. Intel ACE HDA does. */
	assert((stat.write_position & 3) == 0);	/* 4-byte alignment check. */
	/* Read the last received 4 bytes (ignore older ones if any). */
	int last_4_bytes_pos = stat.write_position >= 4 ? (stat.write_position - 4) :
		(dd->uaol.fb_dma_buf_size - 4);
	uint32_t feedback_value = dd->uaol.fb_dma_buf[last_4_bytes_pos / 4];

	ret = sof_dma_reload(dd->dma, dd->uaol.fb_chan_idx, stat.pending_length);
	if (ret < 0) {
		comp_err(dev, "Failed to reload UAOL feedback DMA: %d, pending_length: %d",
		 ret, stat.pending_length);
		return;
	}

	comp_dbg(dev, "UAOL feedback: 0x%x", feedback_value);

	const struct device *uaol_zdev = get_uaol_zdevice(dd->uaol.link_id);
	int freq = uaol_interpret_feedback_value(uaol_zdev, dd->uaol.stream_id, feedback_value);

	if (freq < 0) {
		comp_err(dev, "Unexpected feedback value: 0x%x, err/freq: %d", feedback_value, freq);
		return;
	}

	/* Let's do a sanity check first to see if the received value looks valid.
	 * Then limit the maximum drift to a reasonable value to prevent significant
	 * audio distortion when, for some reason, the reported drift is quite large.
	 */
	#define REJECT_DRIFT_HZ 1000
	#define CLIP_DRIFT_HZ 6

	int drift = freq - dd->ipc_config.sampling_frequency;

	if (drift < -REJECT_DRIFT_HZ || drift > REJECT_DRIFT_HZ) {
		comp_err(dev, "Weird UAOL feedback freq value: %d, drift: %d. Rejected!",
		 freq, drift);
		return;
	}
	if (drift < -CLIP_DRIFT_HZ || drift > CLIP_DRIFT_HZ) {
		comp_warn(dev, "Unreasonable UAOL feedback freq value: %d, drift: %d. Clipped!",
		  freq, drift);
		drift = MAX(-CLIP_DRIFT_HZ, MIN(drift, CLIP_DRIFT_HZ));
	}

	dd->uaol.feedback_drift = drift;
	if (dd->uaol.feedback_drift > 0)
		dsrc_set_rate(&dd->uaol.dsrc, dd->ipc_config.sampling_frequency,
		      drift + dd->ipc_config.sampling_frequency);
}

/* Tells UAOL HW to skip one audio frame or copy one additional audio frame
 * on the next service interval to adjust the data rate.
 */
void adjust_uaol_rate(const struct dai_data *dd, bool increase)
{
	const struct device *uaol_zdev = get_uaol_zdevice(dd->uaol.link_id);
	int ret = uaol_adjust_rate(uaol_zdev, dd->uaol.stream_id, increase);

	if (ret != 0)
		comp_err(dd->dai_dev, "Failed to adjust UAOL rate: %d", ret);
}

/* For UAOL playback with non-zero feedback frequency drift, the UAOL rate should be adjusted
 * periodically to compensate for the drift. Additionally, for positive drift, DSRC is used
 * to resample audio and insert additional audio frame as needed.
 * This function is called from the dai-zephyr dai_dma_cb().
 */
int uaol_dma_buffer_copy_to(struct dai_data *dd, size_t bytes)
{
	int ret = 0;

	assert(dd->uaol.feedback_drift != 0);

	if (dd->uaol.feedback_drift > 0) {
		buffer_stream_invalidate(dd->local_buffer, bytes);

		struct cir_buf_ptr in = { dd->local_buffer->stream.addr,
			dd->local_buffer->stream.end_addr, dd->local_buffer->stream.r_ptr };
		assert(dd->uaol.dsrc_buf);
		struct cir_buf_ptr out = { dd->uaol.dsrc_buf->stream.addr,
			dd->uaol.dsrc_buf->stream.end_addr, dd->uaol.dsrc_buf->stream.w_ptr };
		size_t frames = bytes / audio_stream_frame_bytes(&dd->local_buffer->stream);

		size_t added_frames = dsrc_process(&dd->uaol.dsrc, &in, &out, frames);
		size_t src_added_bytes = added_frames *
			audio_stream_frame_bytes(&dd->local_buffer->stream);

		comp_update_buffer_consume(dd->local_buffer, bytes);
		audio_stream_produce(&dd->uaol.dsrc_buf->stream, bytes + src_added_bytes);

		if (added_frames)
			adjust_uaol_rate(dd, true);

		/* dma_buffer_copy_to() may do format conversion so
		 * dst_added_bytes may not be equal to src_added_bytes.
		 */
		size_t dst_added_bytes = added_frames * audio_stream_frame_bytes(&dd->dma_buffer->stream);
		ret = dma_buffer_copy_to(dd->uaol.dsrc_buf, dd->dma_buffer,
				 dd->process, bytes + dst_added_bytes, dd->chmap);
	} else if (dd->uaol.feedback_drift < 0) {
		/* It is expected this func is called every 1 ms */
		dd->uaol.ms_since_last_adjustment++;
		assert(dd->uaol.feedback_drift < 0 && dd->uaol.feedback_drift >= -1000);
		if (-1000 / dd->uaol.feedback_drift >= dd->uaol.ms_since_last_adjustment) {
			dd->uaol.ms_since_last_adjustment = 0;
			adjust_uaol_rate(dd, false);
		}

		ret = dma_buffer_copy_to(dd->local_buffer, dd->dma_buffer,
				 dd->process, bytes, dd->chmap);
	} else
		return -EINVAL;

	return ret;
}

/* When the host setups two DMA links for UAOL playback gateway, that tells us the second
 * DMA link is to read from the USB feedback endpoint. This function (re)setups the feedback
 * DMA channel and buffer for such case.
 */
int setup_uaol_feedback_dma(struct dai_data *dd, struct comp_dev *dev)
{
	struct ipc_config_dai *dai = &dd->ipc_config;
	struct dma_config *dma_cfg;

	dd->uaol.fb_chan_idx = -EINVAL;
	dd->uaol.feedback_drift = 0;
	dd->uaol.ms_since_last_adjustment = 0;

	if (dai->type != SOF_DAI_INTEL_UAOL || dai->direction != SOF_IPC_STREAM_PLAYBACK)
		return 0;

	/* UAOL feedback is an optional 2nd DMA link (1st is audio DMA link) */
	assert(GTW_DMA_DEVICE_MAX_COUNT >= 2);
	if (!dai->host_dma_config[1] || !dai->host_dma_config[1]->pre_allocated_by_host) {
		comp_info(dev, "No UAOL feedback DMA link supplied by host.");
		return 0;
	}

	int channel = dai->host_dma_config[1]->dma_channel_id;
	comp_dbg(dev, "UAOL feedback channel: %d", channel);

	/* USB feedback endpoint payload is 4 bytes. Hi-Speed USB microframe
	 * period is 125 us (8 per 1 ms). Double the buffer size just in case.
	 */
	dd->uaol.fb_dma_buf_size = 4 * 8 * 2;

	/* TODO: perhaps get alignment by reading DMA alignment attribute? */
	dd->uaol.fb_dma_buf = (uint32_t *)rballoc_align(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_DMA,
						      dd->uaol.fb_dma_buf_size, 64);
	if (!dd->uaol.fb_dma_buf) {
		comp_err(dev, "UAOL feedback buffer allocation failed!");
		return -ENOMEM;
	}
	memset(dd->uaol.fb_dma_buf, 0, dd->uaol.fb_dma_buf_size);

	dma_cfg = rballoc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT | SOF_MEM_FLAG_DMA,
			  sizeof(struct dma_config));
	if (!dma_cfg) {
		rfree(dd->uaol.fb_dma_buf);
		dd->uaol.fb_dma_buf = NULL;
		comp_err(dev, "dma_cfg allocation failed");
		return -ENOMEM;
	}

	memset(dma_cfg, 0, sizeof(struct dma_config));
	dma_cfg->dma_slot = 0;
	dma_cfg->channel_direction = PERIPHERAL_TO_MEMORY;
	dma_cfg->source_data_size = 4;
	dma_cfg->dest_data_size = 4;
	dma_cfg->source_burst_length = 4;
	dma_cfg->dest_burst_length = 4;
	dma_cfg->cyclic = 1;
	dma_cfg->block_count = 1;

	dma_cfg->head_block = rballoc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT | SOF_MEM_FLAG_DMA,
					      sizeof(struct dma_block_config));
	if (!dma_cfg->head_block) {
		rfree(dma_cfg);
		rfree(dd->uaol.fb_dma_buf);
		dd->uaol.fb_dma_buf = NULL;
		comp_err(dev, "dma_block_config allocation failed");
		return -ENOMEM;
	}

	memset(dma_cfg->head_block, 0, sizeof(struct dma_block_config));
	dma_cfg->head_block->dest_scatter_en = 0;
	dma_cfg->head_block->block_size = dd->uaol.fb_dma_buf_size;
	dma_cfg->head_block->source_addr_adj = DMA_ADDR_ADJ_INCREMENT;
	dma_cfg->head_block->dest_addr_adj = DMA_ADDR_ADJ_DECREMENT;	/* WHY? IS THIS OK??? */
	dma_cfg->head_block->source_address = 0;
	dma_cfg->head_block->dest_address = (uint32_t)dd->uaol.fb_dma_buf;
	dma_cfg->head_block->next_block = dma_cfg->head_block;
	dd->uaol.fb_z_config = dma_cfg;

	dd->uaol.fb_chan_idx = sof_dma_request_channel(dd->dma, channel);
	if (dd->uaol.fb_chan_idx < 0) {
		rfree(dma_cfg->head_block);
		rfree(dma_cfg);
		rfree(dd->uaol.fb_dma_buf);
		dd->uaol.fb_dma_buf = NULL;
		comp_err(dev, "dma_request_channel() failed");
		return -EIO;
	}

	dsrc_init(&dd->uaol.dsrc, dai->gtw_fmt->channels_count);

	comp_dbg(dev, "New configured UAOL feedback DMA channel: %d", dd->uaol.fb_chan_idx);

	return 0;
}

void uaol_free(struct dai_data *dd)
{
	if (dd->uaol.fb_z_config) {
		rfree(dd->uaol.fb_z_config->head_block);
		rfree(dd->uaol.fb_z_config);
		dd->uaol.fb_z_config = NULL;
	}

	if (dd->uaol.fb_dma_buf) {
		rfree(dd->uaol.fb_dma_buf);
		dd->uaol.fb_dma_buf = NULL;
		dd->uaol.fb_dma_buf_size = 0;
	}

	if (dd->uaol.dsrc_buf) {
		buffer_free(dd->uaol.dsrc_buf);
		dd->uaol.dsrc_buf = NULL;
	}
}
