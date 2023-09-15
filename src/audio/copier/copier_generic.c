// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>

#include <ipc4/base-config.h>
#include <ipc4/copier.h>
#include <sof/audio/component_ext.h>

LOG_MODULE_DECLARE(copier, CONFIG_SOF_LOG_LEVEL);

#ifdef COPIER_GENERIC

#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <sof/common.h>
#include <stddef.h>
#include <errno.h>
#include <stdint.h>

int apply_attenuation(struct comp_dev *dev, struct copier_data *cd,
		      struct comp_buffer *sink, int frame)
{
	int i;
	int n;
	int nmax;
	int remaining_samples = frame * audio_stream_get_channels(&sink->stream);
	uint32_t bytes = frame * audio_stream_frame_bytes(&sink->stream);
	uint32_t *dst = audio_stream_rewind_wptr_by_bytes(&sink->stream, bytes);

	/* only support attenuation in format of 32bit */
	switch (audio_stream_get_frm_fmt(&sink->stream)) {
	case SOF_IPC_FRAME_S16_LE:
		comp_err(dev, "16bit sample isn't supported by attenuation");
		return -EINVAL;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		while (remaining_samples) {
			nmax = audio_stream_samples_without_wrap_s32(&sink->stream, dst);
			n = MIN(remaining_samples, nmax);
			for (i = 0; i < n; i++) {
				*dst >>= cd->attenuation;
				dst++;
			}
			remaining_samples -= n;
			dst = audio_stream_wrap(&sink->stream, dst);
		}

		return 0;
	default:
		comp_err(dev, "unsupported format %d for attenuation",
			 audio_stream_get_frm_fmt(&sink->stream));
		return -EINVAL;
	}
}
#endif

void copier_update_params(struct copier_data *cd, struct comp_dev *dev,
			  struct sof_ipc_stream_params *params)
{
	struct comp_buffer *sink, *source;
	struct list_item *sink_list;

	memset(params, 0, sizeof(*params));
	params->direction = cd->direction;
	params->channels = cd->config.base.audio_fmt.channels_count;
	params->rate = cd->config.base.audio_fmt.sampling_frequency;
	params->sample_container_bytes = cd->config.base.audio_fmt.depth / 8;
	params->sample_valid_bytes = cd->config.base.audio_fmt.valid_bit_depth / 8;

	params->stream_tag = cd->config.gtw_cfg.node_id.f.v_index + 1;
	params->frame_fmt = dev->ipc_config.frame_fmt;
	params->buffer_fmt = cd->config.base.audio_fmt.interleaving_style;
	params->buffer.size = cd->config.base.ibs;

	/* disable ipc3 stream position */
	params->no_stream_position = 1;

	/* update each sink format */
	list_for_item(sink_list, &dev->bsink_list) {
		int j;

		sink = container_of(sink_list, struct comp_buffer, source_list);

		j = IPC4_SINK_QUEUE_ID(sink->id);

		ipc4_update_buffer_format(sink, &cd->out_fmt[j]);
	}

	/*
	 * force update the source buffer format to cover cases where the source module
	 * fails to set the sink buffer params
	 */
	if (!list_is_empty(&dev->bsource_list)) {
		struct ipc4_audio_format *in_fmt;

		source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);

		in_fmt = &cd->config.base.audio_fmt;
		ipc4_update_buffer_format(source, in_fmt);
	}

	/* update params for the DMA buffer */
	switch (dev->ipc_config.type) {
	case SOF_COMP_HOST:
		if (cd->ipc_gtw || params->direction == SOF_IPC_STREAM_PLAYBACK)
			break;
		COMPILER_FALLTHROUGH;
	case SOF_COMP_DAI:
		if (dev->ipc_config.type == SOF_COMP_DAI &&
		    (cd->endpoint_num > 1 || params->direction == SOF_IPC_STREAM_CAPTURE))
			break;
		params->buffer.size = cd->config.base.obs;
		params->sample_container_bytes = cd->out_fmt->depth / 8;
		params->sample_valid_bytes = cd->out_fmt->valid_bit_depth / 8;
		break;
	default:
		break;
	}
}

int create_endpoint_buffer(struct comp_dev *dev,
			   struct copier_data *cd,
			   const struct ipc4_copier_module_cfg *copier_cfg,
			   bool create_multi_endpoint_buffer)
{
	struct comp_ipc_config *config = &dev->ipc_config;
	enum sof_ipc_frame in_frame_fmt, out_frame_fmt;
	enum sof_ipc_frame in_valid_fmt, out_valid_fmt;
	enum sof_ipc_frame valid_fmt;
	struct sof_ipc_buffer ipc_buf;
	struct comp_buffer *buffer;
	uint32_t buf_size;
	uint32_t chan_map;
	int i;

	audio_stream_fmt_conversion(copier_cfg->base.audio_fmt.depth,
				    copier_cfg->base.audio_fmt.valid_bit_depth,
				    &in_frame_fmt, &in_valid_fmt,
				    copier_cfg->base.audio_fmt.s_type);

	audio_stream_fmt_conversion(copier_cfg->out_fmt.depth,
				    copier_cfg->out_fmt.valid_bit_depth,
				    &out_frame_fmt, &out_valid_fmt,
				    copier_cfg->out_fmt.s_type);

	/* playback case:
	 *
	 * --> copier0 -----> buf1 ----> ....  bufn --------> copier1
	 *        |             /|\               |conversion    |
	 *       \|/             |conversion     \|/            \|/
	 *       host-> endpoint buffer0   endpoint buffer1 ->  dai -->
	 *
	 *  capture case:
	 *
	 *     copier1 <------ bufn <---- ....  buf1 <------- copier0 <--
	 *      |               |conversion     /|\            |
	 *     \|/             \|/               |conversion  \|/
	 * <-- host <- endpoint buffer1   endpoint buffer0 <- dai
	 *
	 * According to above graph, the format of endpoint buffer
	 * depends on stream direction and component type.
	 */
	if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
		if (config->type == SOF_COMP_HOST) {
			config->frame_fmt = in_frame_fmt;
			valid_fmt = in_valid_fmt;
			buf_size = copier_cfg->base.ibs * 2;
		} else {
			config->frame_fmt = out_frame_fmt;
			valid_fmt = out_valid_fmt;
			buf_size = copier_cfg->base.obs * 2;
		}

		chan_map = copier_cfg->out_fmt.ch_map;
	} else {
		if (config->type == SOF_COMP_HOST) {
			config->frame_fmt = out_frame_fmt;
			valid_fmt = out_valid_fmt;
			buf_size = copier_cfg->base.obs * 2;
		} else {
			config->frame_fmt = in_frame_fmt;
			valid_fmt = in_valid_fmt;
			buf_size = copier_cfg->base.ibs * 2;
		}

		chan_map = copier_cfg->base.audio_fmt.ch_map;
	}

	dev->ipc_config.frame_fmt = config->frame_fmt;

	memset(&ipc_buf, 0, sizeof(ipc_buf));
	ipc_buf.size = buf_size;
	ipc_buf.comp.pipeline_id = config->pipeline_id;
	ipc_buf.comp.core = config->core;
	/* allocate not shared buffer */
	buffer = buffer_new(&ipc_buf, false);
	if (!buffer)
		return -ENOMEM;

	audio_stream_set_channels(&buffer->stream, copier_cfg->base.audio_fmt.channels_count);
	audio_stream_set_rate(&buffer->stream, copier_cfg->base.audio_fmt.sampling_frequency);
	audio_stream_set_frm_fmt(&buffer->stream, config->frame_fmt);
	audio_stream_set_valid_fmt(&buffer->stream, valid_fmt);
	audio_stream_set_buffer_fmt(&buffer->stream,
				    copier_cfg->base.audio_fmt.interleaving_style);

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		buffer->chmap[i] = (chan_map >> i * 4) & 0xf;

	buffer->hw_params_configured = true;

	if (create_multi_endpoint_buffer)
		cd->multi_endpoint_buffer = buffer;
	else
		cd->endpoint_buffer[cd->endpoint_num] = buffer;

	return 0;
}

enum sof_ipc_stream_direction
	get_gateway_direction(enum ipc4_connector_node_id_type node_id_type)
{
	/* WARNING: simple "% 2" formula that was used before does not work for all
	 * interfaces: at least it does not work for IPC gateway. But it may also
	 * does not work for other not yet supported interfaces. And so additional
	 * cases might be required here in future.
	 */
	switch (node_id_type) {
	/* from DSP to host */
	case ipc4_ipc_output_class:
		return SOF_IPC_STREAM_CAPTURE;
	/* from host to DSP */
	case ipc4_ipc_input_class:
		return SOF_IPC_STREAM_PLAYBACK;
	default:
		return node_id_type % 2;
	}
}

/* In sof normal format conversion path, sample size should be equal
 * to container size except format of S24_LE. In ipc4 case, sample
 * size can be different with container size. This function is used to
 * check conversion mode.
 */
static bool use_no_container_convert_function(enum sof_ipc_frame in,
					      enum sof_ipc_frame valid_in_bits,
					      enum sof_ipc_frame out,
					      enum sof_ipc_frame valid_out_bits)
{
	/* valid sample size is equal to container size, go normal path */
	if (in == valid_in_bits && out == valid_out_bits) {
		if (in == SOF_IPC_FRAME_S24_3LE || out == SOF_IPC_FRAME_S24_3LE)
			return false;

		return true;
	}

	return false;
}

pcm_converter_func get_converter_func(const struct ipc4_audio_format *in_fmt,
				      const struct ipc4_audio_format *out_fmt,
				      enum ipc4_gateway_type type,
				      enum ipc4_direction_type dir)
{
	enum sof_ipc_frame in, in_valid, out, out_valid;

	audio_stream_fmt_conversion(in_fmt->depth, in_fmt->valid_bit_depth, &in, &in_valid,
				    in_fmt->s_type);
	audio_stream_fmt_conversion(out_fmt->depth, out_fmt->valid_bit_depth, &out, &out_valid,
				    out_fmt->s_type);

	if (in_fmt->s_type == IPC4_TYPE_MSB_INTEGER && in_valid == SOF_IPC_FRAME_S24_4LE)
		in_valid = SOF_IPC_FRAME_S24_4LE_MSB;
	if (out_fmt->s_type == IPC4_TYPE_MSB_INTEGER && out_valid == SOF_IPC_FRAME_S24_4LE)
		out_valid = SOF_IPC_FRAME_S24_4LE_MSB;

	/* check container & sample size */
	if (use_no_container_convert_function(in, in_valid, out, out_valid))
		return pcm_get_conversion_function(in, out);
	else
		return pcm_get_conversion_vc_function(in, in_valid, out, out_valid, type, dir);
}
