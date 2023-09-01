// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2023 Intel Corporation. All rights reserved.
//
// Author: Baofeng Tian <baofeng.tian@intel.com>

#include <sof/trace/trace.h>
#include <sof/audio/component_ext.h>
#include <ipc4/copier.h>
#include <sof/audio/dai_copier.h>
#include <ipc/dai.h>
#include <sof/audio/module_adapter/module/generic.h>

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
	struct processing_module *mod = comp_get_drvdata(dev);
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

static int copier_dai_init(struct comp_dev *dev,
			   struct comp_ipc_config *config,
			   const struct ipc4_copier_module_cfg *copier,
			   struct pipeline *pipeline,
			   struct ipc_config_dai *dai,
			   enum ipc4_gateway_type type,
			   int index, int dai_count)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct copier_data *cd = module_get_private_data(mod);
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
	} else {
		enum sof_ipc_frame in_frame_fmt, in_valid_fmt;

		audio_stream_fmt_conversion(copier->base.audio_fmt.depth,
					    copier->base.audio_fmt.valid_bit_depth,
					    &in_frame_fmt, &in_valid_fmt,
					    copier->base.audio_fmt.s_type);
		config->frame_fmt = in_frame_fmt;
		pipeline->source_comp = dev;
	}

	/* save the channel map and count for ALH multi-gateway */
	if (type == ipc4_gtw_alh && is_multi_gateway(copier->gtw_cfg.node_id)) {
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

	pipeline->sched_id = config->id;

	cd->dd[index] = dd;
	ret = comp_dai_config(cd->dd[index], dev, dai, copier);
	if (ret < 0)
		goto e_zephyr_free;

	cd->endpoint_num++;

	return 0;
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
	struct processing_module *mod = comp_get_drvdata(dev);
	struct comp_ipc_config *config = &dev->ipc_config;
	int dai_index[IPC4_ALH_MAX_NUMBER_OF_GTW];
	union ipc4_connector_node_id node_id;
	enum ipc4_gateway_type type;
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
		type = ipc4_gtw_link;
		break;
	case ipc4_i2s_link_output_class:
	case ipc4_i2s_link_input_class:
		dai_index[dai_count - 1] = (dai_index[dai_count - 1] >> 4) & 0xF;
		dai.type = SOF_DAI_INTEL_SSP;
		dai.is_config_blob = true;
		type = ipc4_gtw_ssp;
		ret = ipc4_find_dma_config(&dai, (uint8_t *)copier->gtw_cfg.config_data,
					   copier->gtw_cfg.config_length * 4);
		if (ret != 0) {
			comp_err(dev, "No ssp dma_config found in blob!");
			return -EINVAL;
		}
		dai.out_fmt = &copier->out_fmt;
		break;
	case ipc4_alh_link_output_class:
	case ipc4_alh_link_input_class:
		dai.type = SOF_DAI_INTEL_ALH;
		dai.is_config_blob = true;
		type = ipc4_gtw_alh;

		/* copier
		 * {
		 *  gtw_cfg
		 *  {
		 *     gtw_node_id;
		 *     config_length;
		 *     config_data
		 *     {
		 *	   count;
		 *	  {
		 *	     node_id;  \\ normal gtw id
		 *	     mask;
		 *	  }  mapping[MAX_ALH_COUNT];
		 *     }
		 *   }
		 * }
		 */
		 /* get gtw node id in config data */
		if (is_multi_gateway(node_id)) {
			if (copier->gtw_cfg.config_length) {
				const struct sof_alh_configuration_blob *alh_blob =
					(const struct sof_alh_configuration_blob *)
						copier->gtw_cfg.config_data;

				dai_count = alh_blob->alh_cfg.count;
				if (dai_count > IPC4_ALH_MAX_NUMBER_OF_GTW || dai_count < 0) {
					comp_err(dev, "Invalid dai_count: %d", dai_count);
					return -EINVAL;
				}
				for (i = 0; i < dai_count; i++)
					dai_index[i] =
					IPC4_ALH_DAI_INDEX(alh_blob->alh_cfg.mapping[i].alh_id);
			} else {
				comp_err(dev, "No ipc4_alh_multi_gtw_cfg found in blob!");
				return -EINVAL;
			}
		} else {
			dai_index[dai_count - 1] = IPC4_ALH_DAI_INDEX(node_id.f.v_index);
		}

		break;
	case ipc4_dmic_link_input_class:
		dai.type = SOF_DAI_INTEL_DMIC;
		dai.is_config_blob = true;
		type = ipc4_gtw_dmic;

		ret = ipc4_find_dma_config(&dai, (uint8_t *)copier->gtw_cfg.config_data,
					   copier->gtw_cfg.config_length * 4);
		if (ret != 0) {
			comp_err(dev, "No dmic dma_config found in blob!");
			return -EINVAL;
		}
		dai.out_fmt = &copier->out_fmt;
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < dai_count; i++) {
		dai.dai_index = dai_index[i];
		ret = copier_dai_init(dev, config, copier, pipeline, &dai, type, i,
				      dai_count);
		if (ret) {
			comp_err(dev, "failed to create dai");
			return ret;
		}
	}

	cd->converter[IPC4_COPIER_GATEWAY_PIN] =
			get_converter_func(&copier->base.audio_fmt, &copier->out_fmt, type,
					   IPC4_DIRECTION(dai.direction));
	if (!cd->converter[IPC4_COPIER_GATEWAY_PIN]) {
		comp_err(dev, "failed to get converter type %d, dir %d",
			 type, dai.direction);
		return -EINVAL;
	}

	/* create multi_endpoint_buffer for ALH multi-gateway case */
	if (dai_count > 1) {
		ret = create_endpoint_buffer(dev, cd, copier, true);
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

int copier_dai_params(struct copier_data *cd, struct comp_dev *dev,
		      struct sof_ipc_stream_params *params, int dai_index)
{
	struct sof_ipc_stream_params demuxed_params = *params;
	const struct ipc4_audio_format *in_fmt = &cd->config.base.audio_fmt;
	const struct ipc4_audio_format *out_fmt = &cd->config.out_fmt;
	enum sof_ipc_frame in_bits, in_valid_bits, out_bits, out_valid_bits;
	int container_size;
	int j, ret;

	if (cd->endpoint_num == 1) {
		ret = dai_common_params(cd->dd[0], dev, params);

		/*
		 * dai_zephyr_params assigns the conversion function
		 * based on the input/output formats but does not take
		 * the valid bits into account. So change the conversion
		 * function if the valid bits are different from the
		 * container size.
		 */
		audio_stream_fmt_conversion(in_fmt->depth,
					    in_fmt->valid_bit_depth,
					    &in_bits, &in_valid_bits,
					    in_fmt->s_type);
		audio_stream_fmt_conversion(out_fmt->depth,
					    out_fmt->valid_bit_depth,
					    &out_bits, &out_valid_bits,
					    out_fmt->s_type);

		if (in_bits != in_valid_bits || out_bits != out_valid_bits)
			cd->dd[0]->process =
				cd->converter[IPC4_COPIER_GATEWAY_PIN];
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
		cd->dd[dai_index]->dma_buffer->chmap[j] = (cd->chan_map[dai_index] >> j * 4) & 0xf;

	/* set channel copy func */
	container_size = audio_stream_sample_bytes(&cd->multi_endpoint_buffer->stream);

	switch (container_size) {
	case 2:
		cd->dd[dai_index]->process = copy_single_channel_c16;
		break;
	case 4:
		cd->dd[dai_index]->process = copy_single_channel_c32;
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
