// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2023 Intel Corporation. All rights reserved.
//
// Author: Baofeng Tian <baofeng.tian@intel.com>

#include <sof/trace/trace.h>
#include <sof/audio/component_ext.h>
#include <ipc/dai.h>
#include <sof/audio/module_adapter/module/generic.h>
#include "copier.h"
#include "dai_copier.h"
#include "copier_gain.h"

LOG_MODULE_DECLARE(copier, CONFIG_SOF_LOG_LEVEL);

static uint32_t bitmask_to_nibble_channel_map(uint8_t bitmask)
{
	int i;
	int channel_count = 0;
	uint32_t nibble_map = 0;

	for (i = 0; i < 8; i++)
		if (bitmask & BIT(i)) {
			nibble_map |= i << (channel_count * 4);
			channel_count++;
		}

	/* absent channel is represented as 0xf nibble */
	nibble_map |= 0xFFFFFFFF << (channel_count * 4);

	return nibble_map;
}

static int copier_set_alh_multi_gtw_channel_map(struct comp_dev *dev,
						const struct ipc4_copier_module_cfg *copier_cfg,
						int index)
{
	struct processing_module *mod = comp_mod(dev);
	struct copier_data *cd = module_get_private_data(mod);
	const struct sof_alh_configuration_blob *alh_blob;
	uint8_t chan_bitmask;
	int channels;

	if (!copier_cfg->gtw_cfg.config_length) {
		comp_err(dev, "No ipc4_alh_multi_gtw_cfg found in blob!");
		return -EINVAL;
	}

	/* For ALH multi-gateway case, configuration blob contains struct ipc4_alh_multi_gtw_cfg
	 * with channel map and channels number for each individual gateway.
	 */
	alh_blob = (const struct sof_alh_configuration_blob *)copier_cfg->gtw_cfg.config_data;
	chan_bitmask = alh_blob->alh_cfg.mapping[index].channel_mask;

	channels = popcount(chan_bitmask);
	if (channels < 1 || channels > SOF_IPC_MAX_CHANNELS) {
		comp_err(dev, "Invalid channels mask: 0x%x", chan_bitmask);
		return -EINVAL;
	}

	cd->channels[index] = channels;
	cd->chan_map[index] = bitmask_to_nibble_channel_map(chan_bitmask);

	return 0;
}

static int copier_alh_assign_dai_index(struct comp_dev *dev,
				       void *gtw_cfg_data,
				       union ipc4_connector_node_id node_id,
				       struct ipc_config_dai *dai,
				       int *dai_index,
				       int *dai_count)
{
	struct processing_module *mod = comp_mod(dev);
	struct copier_data *cd = module_get_private_data(mod);
	const struct sof_alh_configuration_blob *alh_blob = gtw_cfg_data;
	uint8_t *dma_config;
	size_t alh_cfg_size, dma_config_length;
	int i, dai_num, ret;

	if (!cd->config.gtw_cfg.config_length) {
		comp_err(mod->dev, "No gateway config found in blob!");
		return -EINVAL;
	}

	switch (dai->type) {
	case SOF_DAI_INTEL_HDA:
		/* We use DAI_INTEL_HDA for ACE 2.0 platforms */
		alh_cfg_size = get_alh_config_size(alh_blob);
		dma_config = (uint8_t *)gtw_cfg_data + alh_cfg_size;
		dma_config_length = (cd->config.gtw_cfg.config_length << 2) - alh_cfg_size;

		/* Here we check node_id if we need to use FW aggregation,
		 * in other words do we need to create multiple dai or not
		 */
		if (!is_multi_gateway(node_id)) {
			/* Find DMA config in blob and retrieve stream_id */
			ret = ipc4_find_dma_config_multiple(dai, dma_config, dma_config_length,
							    alh_blob->alh_cfg.mapping[0].alh_id, 0);
			if (ret != 0) {
				comp_err(mod->dev, "No sndw dma_config found in blob!");
				return -EINVAL;
			}
			dai_index[0] = dai->host_dma_config[0]->stream_id;
			return 0;
		}

		dai_num = alh_blob->alh_cfg.count;
		if (dai_num > IPC4_ALH_MAX_NUMBER_OF_GTW || dai_num < 0) {
			comp_err(mod->dev, "Invalid dai_count: %d", dai_num);
			return -EINVAL;
		}

		for (i = 0; i < dai_num; i++) {
			ret = ipc4_find_dma_config_multiple(dai, dma_config,
							    dma_config_length,
							    alh_blob->alh_cfg.mapping[i].alh_id, i);
			if (ret != 0) {
				comp_err(mod->dev, "No sndw dma_config found in blob!");
				return -EINVAL;
			}

			/* To process data on SoundWire interface HD-A DMA is used so it seems
			 * logical to me to use stream tag as a dai_index instead of PDI.
			 */
			dai_index[i] = dai->host_dma_config[i]->stream_id;
		}

		*dai_count = dai_num;
		break;
	case SOF_DAI_INTEL_ALH:
		/* Use DAI_INTEL_ALH for ACE 1.0 and older */
		if (!is_multi_gateway(node_id)) {
			dai_index[0] = IPC4_ALH_DAI_INDEX(node_id.f.v_index);
			return 0;
		}

		dai_num = alh_blob->alh_cfg.count;
		if (dai_num > IPC4_ALH_MAX_NUMBER_OF_GTW || dai_num < 0) {
			comp_err(mod->dev, "Invalid dai_count: %d", dai_num);
			return -EINVAL;
		}

		for (i = 0; i < dai_num; i++)
			dai_index[i] = IPC4_ALH_DAI_INDEX(alh_blob->alh_cfg.mapping[i].alh_id);

		*dai_count = dai_num;
		break;
	default:
		comp_err(mod->dev, "Invalid dai type selected: %d", dai->type);
		return -EINVAL;
	}

	return 0;
}

static int copier_dai_init(struct comp_dev *dev,
			   struct comp_ipc_config *config,
			   const struct ipc4_copier_module_cfg *copier,
			   struct pipeline *pipeline,
			   struct ipc_config_dai *dai,
			   enum ipc4_gateway_type type,
			   int index, int dai_count)
{
	struct processing_module *mod = comp_mod(dev);
	struct copier_data *cd = module_get_private_data(mod);
	uint32_t chmap;
	struct dai_data *dd;
	int ret;

	if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
		enum sof_ipc_frame out_frame_fmt, out_valid_fmt;

		audio_stream_fmt_conversion(copier->out_fmt.depth,
					    copier->out_fmt.valid_bit_depth,
					    &out_frame_fmt,
					    &out_valid_fmt,
					    copier->out_fmt.s_type);
		config->frame_fmt = out_frame_fmt;
		pipeline->sink_comp = dev;
		cd->bsource_buffer = true;
		chmap = copier->base.audio_fmt.ch_map;
	} else {
		enum sof_ipc_frame in_frame_fmt, in_valid_fmt;

		audio_stream_fmt_conversion(copier->base.audio_fmt.depth,
					    copier->base.audio_fmt.valid_bit_depth,
					    &in_frame_fmt, &in_valid_fmt,
					    copier->base.audio_fmt.s_type);
		config->frame_fmt = in_frame_fmt;
		pipeline->source_comp = dev;
		chmap = copier->out_fmt.ch_map;
	}

	/* save the channel map and count for ALH multi-gateway */
	if ((type == ipc4_gtw_alh || type == ipc4_gtw_link) &&
	    is_multi_gateway(copier->gtw_cfg.node_id)) {
		ret = copier_set_alh_multi_gtw_channel_map(dev, copier, index);
		if (ret < 0)
			return ret;
	}

	dd = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*dd));
	if (!dd)
		return -ENOMEM;

	ret = dai_common_new(dd, dev, dai);
	if (ret < 0)
		goto free_dd;

	dd->chmap = chmap;

	pipeline->sched_id = config->id;

	cd->dd[index] = dd;
	ret = comp_dai_config(cd->dd[index], dev, dai, copier);
	if (ret < 0)
		goto e_zephyr_free;

	/* Allocate gain data if selected for this dai type and set basic params */
	if (dai->apply_gain) {
		struct copier_gain_params *gain_data = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED,
							       0, SOF_MEM_CAPS_RAM,
							       sizeof(*gain_data));
		if (!gain_data) {
			ret = -ENOMEM;
			goto e_zephyr_free;
		}
		cd->dd[index]->gain_data = gain_data;

		ret = copier_gain_set_params(dev, cd->dd[index]);
		if (ret < 0) {
			comp_err(dev, "Failed to set gain params!");
			goto gain_free;
		}
	}

	cd->endpoint_num++;

	return 0;
gain_free:
	rfree(dd->gain_data);
e_zephyr_free:
	dai_common_free(dd);
free_dd:
	rfree(dd);
	return ret;
}

/* if copier is linked to non-host gateway, it will manage link dma,
 * ssp, dmic or alh. Sof dai component can support this case so copier
 * reuses dai component to support non-host gateway.
 */
int copier_dai_create(struct comp_dev *dev, struct copier_data *cd,
		      const struct ipc4_copier_module_cfg *copier,
		      struct pipeline *pipeline)
{
	struct processing_module *mod = comp_mod(dev);
	struct comp_ipc_config *config = &dev->ipc_config;
	int dai_index[IPC4_ALH_MAX_NUMBER_OF_GTW];
	union ipc4_connector_node_id node_id;
	struct ipc_config_dai dai;
	int dai_count;
	int i, ret;

	config->type = SOF_COMP_DAI;

	memset(&dai, 0, sizeof(dai));
	dai_count = 1;
	node_id = copier->gtw_cfg.node_id;
	dai_index[dai_count - 1] = node_id.f.v_index;
	dai.direction = get_gateway_direction(node_id.f.dma_type);
	dai.is_config_blob = true;
	dai.sampling_frequency = copier->out_fmt.sampling_frequency;
	dai.feature_mask = copier->copier_feature_mask;

	switch (node_id.f.dma_type) {
	case ipc4_hda_link_output_class:
	case ipc4_hda_link_input_class:
		dai.type = SOF_DAI_INTEL_HDA;
		dai.is_config_blob = true;
		cd->gtw_type = ipc4_gtw_link;
		break;
	case ipc4_i2s_link_output_class:
	case ipc4_i2s_link_input_class:
		dai.type = SOF_DAI_INTEL_SSP;
		dai.is_config_blob = true;
		cd->gtw_type = ipc4_gtw_ssp;
		ret = ipc4_find_dma_config(&dai, (uint8_t *)cd->gtw_cfg,
					   copier->gtw_cfg.config_length * 4);
		if (ret != 0) {
			comp_err(dev, "No ssp dma_config found in blob!");
			return -EINVAL;
		}
		dai.out_fmt = &copier->out_fmt;
		break;
	case ipc4_alh_link_output_class:
	case ipc4_alh_link_input_class:
#if ACE_VERSION > ACE_VERSION_1_5
		dai.type = SOF_DAI_INTEL_HDA;
		dai.is_config_blob = true;
		cd->gtw_type = ipc4_gtw_link;
#else
		dai.type = SOF_DAI_INTEL_ALH;
		dai.is_config_blob = true;
		cd->gtw_type = ipc4_gtw_alh;
#endif /* ACE_VERSION > ACE_VERSION_1_5 */
		ret = copier_alh_assign_dai_index(dev, cd->gtw_cfg, node_id,
						  &dai, dai_index, &dai_count);
		if (ret)
			return ret;
		break;
	case ipc4_dmic_link_input_class:
		dai.type = SOF_DAI_INTEL_DMIC;
		dai.is_config_blob = true;
		cd->gtw_type = ipc4_gtw_dmic;
		ret = ipc4_find_dma_config(&dai, (uint8_t *)cd->gtw_cfg,
					   copier->gtw_cfg.config_length * 4);
		if (ret != 0) {
			comp_err(dev, "No dmic dma_config found in blob!");
			return -EINVAL;
		}
		dai.out_fmt = &copier->out_fmt;
#if CONFIG_COPIER_GAIN
		dai.apply_gain = true;
#endif
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < dai_count; i++) {
		dai.dai_index = dai_index[i];
		ret = copier_dai_init(dev, config, copier, pipeline, &dai, cd->gtw_type, i,
				      dai_count);
		if (ret) {
			comp_err(dev, "failed to create dai");
			return ret;
		}
	}

	cd->converter[IPC4_COPIER_GATEWAY_PIN] =
			get_converter_func(&copier->base.audio_fmt, &copier->out_fmt, cd->gtw_type,
					   IPC4_DIRECTION(dai.direction), DUMMY_CHMAP);
	if (!cd->converter[IPC4_COPIER_GATEWAY_PIN]) {
		comp_err(dev, "failed to get converter type %d, dir %d",
			 cd->gtw_type, dai.direction);
		return -EINVAL;
	}

	/* create multi_endpoint_buffer for ALH multi-gateway case */
	if (dai_count > 1) {
		ret = create_multi_endpoint_buffer(dev, cd, copier);
		if (ret < 0)
			return ret;
	}

	if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
		pipeline->sink_comp = dev;
	} else {
		pipeline->source_comp = dev;

		/* set max sink count for capture */
		mod->max_sinks = IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT;
	}

	return 0;
}

void copier_dai_free(struct copier_data *cd)
{
	for (int i = 0; i < cd->endpoint_num; i++) {
		dai_common_free(cd->dd[i]);
		rfree(cd->dd[i]->gain_data);
		rfree(cd->dd[i]);
	}
	/* only dai have multi endpoint case */
	if (cd->multi_endpoint_buffer)
		buffer_free(cd->multi_endpoint_buffer);
}

int copier_dai_prepare(struct comp_dev *dev, struct copier_data *cd)
{
	int ret;

	for (int i = 0; i < cd->endpoint_num; i++) {
		ret = dai_common_config_prepare(cd->dd[i], dev);
		if (ret < 0)
			return ret;

		ret = dai_common_prepare(cd->dd[i], dev);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int copy_single_channel_c16(const struct audio_stream *src,
				   unsigned int src_channel,
				   struct audio_stream *dst,
				   unsigned int dst_channel, unsigned int frame_count)
{
	int16_t *r_ptr = (int16_t *)audio_stream_get_rptr(src) + src_channel;
	int16_t *w_ptr = (int16_t *)audio_stream_get_wptr(dst) + dst_channel;

	/* We have to iterate over frames here. However, tracking frames requires using
	 * of expensive division operations (e.g., inside audio_stream_frames_without_wrap()).
	 * So let's track samples instead. Since we only copy one channel, src_stream_sample_count
	 * is NOT number of samples we need to copy but total samples for all channels. We just
	 * track them to know when to stop.
	 */
	int src_stream_sample_count = frame_count * audio_stream_get_channels(src);

	while (src_stream_sample_count) {
		int src_samples_without_wrap;
		int16_t *r_end_ptr, *r_ptr_before_loop;

		r_ptr = audio_stream_wrap(src, r_ptr);
		w_ptr = audio_stream_wrap(dst, w_ptr);

		src_samples_without_wrap = audio_stream_samples_without_wrap_s16(src, r_ptr);
		r_end_ptr = src_stream_sample_count < src_samples_without_wrap ?
			r_ptr + src_stream_sample_count : (int16_t *)audio_stream_get_end_addr(src);

		r_ptr_before_loop = r_ptr;

		do {
			*w_ptr = *r_ptr;
			r_ptr += audio_stream_get_channels(src);
			w_ptr += audio_stream_get_channels(dst);
		} while (r_ptr < r_end_ptr && w_ptr < (int16_t *)audio_stream_get_end_addr(dst));

		src_stream_sample_count -= r_ptr - r_ptr_before_loop;
	}

	return 0;
}

static int copy_single_channel_c32(const struct audio_stream *src,
				   unsigned int src_channel,
				   struct audio_stream *dst,
				   unsigned int dst_channel, unsigned int frame_count)
{
	int32_t *r_ptr = (int32_t *)audio_stream_get_rptr(src) + src_channel;
	int32_t *w_ptr = (int32_t *)audio_stream_get_wptr(dst) + dst_channel;

	/* We have to iterate over frames here. However, tracking frames requires using
	 * of expensive division operations (e.g., inside audio_stream_frames_without_wrap()).
	 * So let's track samples instead. Since we only copy one channel, src_stream_sample_count
	 * is NOT number of samples we need to copy but total samples for all channels. We just
	 * track them to know when to stop.
	 */
	int src_stream_sample_count = frame_count * audio_stream_get_channels(src);

	while (src_stream_sample_count) {
		int src_samples_without_wrap;
		int32_t *r_end_ptr, *r_ptr_before_loop;

		r_ptr = audio_stream_wrap(src, r_ptr);
		w_ptr = audio_stream_wrap(dst, w_ptr);

		src_samples_without_wrap = audio_stream_samples_without_wrap_s32(src, r_ptr);
		r_end_ptr = src_stream_sample_count < src_samples_without_wrap ?
			r_ptr + src_stream_sample_count : (int32_t *)audio_stream_get_end_addr(src);

		r_ptr_before_loop = r_ptr;

		do {
			*w_ptr = *r_ptr;
			r_ptr += audio_stream_get_channels(src);
			w_ptr += audio_stream_get_channels(dst);
		} while (r_ptr < r_end_ptr && w_ptr < (int32_t *)audio_stream_get_end_addr(dst));

		src_stream_sample_count -= r_ptr - r_ptr_before_loop;
	}

	return 0;
}

void copier_dai_adjust_params(const struct copier_data *cd,
			      struct ipc4_audio_format *in_fmt,
			      struct ipc4_audio_format *out_fmt)
{
	struct comp_buffer *dma_buf;
	int dma_buf_channels;
	int dma_buf_container_bits, dma_buf_valid_bits;

	/* Call this func only for DAI gateway with already setup DMA buffer */
	assert(cd->dd[0] && cd->dd[0]->dma_buffer);
	dma_buf = cd->dd[0]->dma_buffer;

	/* Unfortunately, configuring the gateway DMA buffer format is somewhat confusing.
	 * The number of channels can come from hardware parameters (extracted from a blob?)
	 * and also appears in the copier's input/output format. In case the value returned
	 * by the hardware looks valid, it should take precedence over the value from the
	 * copier's input/output format.
	 *
	 * The frame format comes from the topology as dev->ipc_config.frame_fmt and also
	 * comes as the copier's input/output format. The logic is confusing: the format
	 * from the topology takes priority, except when the copier's format container and
	 * valid sample size are different. Perhaps this is to support the 16-bit valid
	 * in the 32-bit container format used by SSP, as such a format cannot be specified
	 * in the topology?
	 */
	dma_buf_channels = audio_stream_get_channels(&dma_buf->stream);
	dma_buf_container_bits = audio_stream_sample_bytes(&dma_buf->stream) * 8;
	dma_buf_valid_bits = get_sample_bitdepth(audio_stream_get_frm_fmt(&dma_buf->stream));

	if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
		out_fmt->channels_count = dma_buf_channels;

		if (!(dma_buf_container_bits == out_fmt->depth &&
		      out_fmt->depth != out_fmt->valid_bit_depth)) {
			out_fmt->depth = dma_buf_container_bits;
			out_fmt->valid_bit_depth = dma_buf_valid_bits;
		}
	} else {
		in_fmt->channels_count = dma_buf_channels;

		if (!(dma_buf_container_bits == in_fmt->depth &&
		      in_fmt->depth != in_fmt->valid_bit_depth)) {
			in_fmt->depth = dma_buf_container_bits;
			in_fmt->valid_bit_depth = dma_buf_valid_bits;
		}
	}
}

int copier_dai_params(struct copier_data *cd, struct comp_dev *dev,
		      struct sof_ipc_stream_params *params, int dai_index)
{
	struct sof_ipc_stream_params demuxed_params = *params;
	int container_size;
	int j, ret;

	if (cd->endpoint_num == 1) {
		struct ipc4_audio_format in_fmt = cd->config.base.audio_fmt;
		struct ipc4_audio_format out_fmt = cd->config.out_fmt;
		enum ipc4_direction_type dir;

		ret = dai_common_params(cd->dd[0], dev, params);
		if (ret < 0)
			return ret;

		copier_dai_adjust_params(cd, &in_fmt, &out_fmt);

		dir = (cd->direction == SOF_IPC_STREAM_PLAYBACK) ?
			ipc4_playback : ipc4_capture;

		cd->dd[0]->process =
			get_converter_func(&in_fmt, &out_fmt, cd->gtw_type, dir, cd->dd[0]->chmap);

		return ret;
	}

	/* For ALH multi-gateway case, params->channels is a total multiplexed
	 * number of channels. Demultiplexed number of channels for each individual
	 * gateway comes in blob's struct ipc4_alh_multi_gtw_cfg.
	 */
	demuxed_params.channels = cd->channels[dai_index];

	ret = dai_common_params(cd->dd[dai_index], dev, &demuxed_params);
	if (ret < 0)
		return ret;

	for (j = 0; j < SOF_IPC_MAX_CHANNELS; j++)
		audio_buffer_set_chmap(&cd->dd[dai_index]->dma_buffer->audio_buffer,
				       j, (cd->chan_map[dai_index] >> j * 4) & 0xf);

	/* set channel copy func */
	container_size = audio_stream_sample_bytes(&cd->multi_endpoint_buffer->stream);

	switch (container_size) {
	case 2:
		cd->dd[dai_index]->channel_copy = copy_single_channel_c16;
		break;
	case 4:
		cd->dd[dai_index]->channel_copy = copy_single_channel_c32;
		break;
	default:
		comp_err(dev, "Unexpected container size: %d", container_size);
		return -EINVAL;
	}

	return ret;
}

void copier_dai_reset(struct copier_data *cd, struct comp_dev *dev)
{
	for (int i = 0; i < cd->endpoint_num; i++)
		dai_common_reset(cd->dd[i], dev);
}

int copier_dai_trigger(struct copier_data *cd, struct comp_dev *dev, int cmd)
{
	int ret;

	for (int i = 0; i < cd->endpoint_num; i++) {
		ret = dai_common_trigger(cd->dd[i], dev, cmd);
		if (ret < 0)
			return ret;
	}
	return 0;
}
