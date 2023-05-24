// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>

#include <ipc4/copier.h>
#include <sof/audio/component_ext.h>

#ifdef COPIER_GENERIC

#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <sof/common.h>
#include <stddef.h>
#include <errno.h>
#include <stdint.h>

LOG_MODULE_REGISTER(copier_generic, CONFIG_SOF_LOG_LEVEL);

int apply_attenuation(struct comp_dev *dev, struct copier_data *cd,
		      struct comp_buffer __sparse_cache *sink, int frame)
{
	int i;
	int n;
	int nmax;
	int remaining_samples = frame * audio_stream_get_channels(&sink->stream);
	int32_t *dst = audio_stream_get_rptr(&sink->stream);

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

void update_buffer_format(struct comp_buffer __sparse_cache *buf_c,
			  const struct ipc4_audio_format *fmt)
{
	enum sof_ipc_frame valid_fmt, frame_fmt;
	int i;

	buf_c->stream.channels = fmt->channels_count;
	buf_c->stream.rate = fmt->sampling_frequency;
	audio_stream_fmt_conversion(fmt->depth,
				    fmt->valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    fmt->s_type);

	buf_c->stream.frame_fmt = frame_fmt;
	buf_c->stream.valid_sample_fmt = valid_fmt;

	buf_c->buffer_fmt = fmt->interleaving_style;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		buf_c->chmap[i] = (fmt->ch_map >> i * 4) & 0xf;

	buf_c->hw_params_configured = true;
}

void copier_update_params(struct copier_data *cd, struct comp_dev *dev,
			  struct sof_ipc_stream_params *params)
{
	struct comp_buffer *sink, *source;
	struct comp_buffer __sparse_cache *sink_c, *source_c;
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
		sink_c = buffer_acquire(sink);

		j = IPC4_SINK_QUEUE_ID(sink_c->id);

		update_buffer_format(sink_c, &cd->out_fmt[j]);

		buffer_release(sink_c);
	}

	/*
	 * force update the source buffer format to cover cases where the source module
	 * fails to set the sink buffer params
	 */
	if (!list_is_empty(&dev->bsource_list)) {
		struct ipc4_audio_format *in_fmt;

		source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		source_c = buffer_acquire(source);

		in_fmt = &cd->config.base.audio_fmt;
		update_buffer_format(source_c, in_fmt);

		buffer_release(source_c);
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
