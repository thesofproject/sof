// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>

#include <ipc4/base-config.h>
#include <sof/audio/component_ext.h>
#include <sof/lib/memory.h>
#include <module/module/base.h>
#include <sof/common.h>
#include <ipc/dai.h>
#include "copier.h"

LOG_MODULE_DECLARE(copier, CONFIG_SOF_LOG_LEVEL);

#if SOF_USE_HIFI(NONE, COPIER)

#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include "copier_gain.h"

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

__cold void copier_gain_set_basic_params(struct comp_dev *dev,
					 struct copier_gain_params *gain_params,
					 struct ipc4_base_module_cfg *ipc4_cfg)
{
	assert_can_be_cold();

	gain_params->channels_count = ipc4_cfg->audio_fmt.channels_count;

	for (int i = 0; i < MAX_GAIN_COEFFS_CNT; i++)
		gain_params->gain_coeffs[i] = UNITY_GAIN_GENERIC;
}

__cold int copier_gain_set_fade_params(struct comp_dev *dev, struct copier_gain_params *gain_params,
				       struct ipc4_base_module_cfg *ipc4_cfg,
				       uint32_t fade_period, uint32_t frames)
{
	uint16_t step_i64_to_i16;

	assert_can_be_cold();

	if (fade_period == GAIN_DEFAULT_FADE_PERIOD) {
		/* Set fade transition delay to default value*/
		if (ipc4_cfg->audio_fmt.sampling_frequency > IPC4_FS_16000HZ)
			gain_params->fade_sg_length = frames * GAIN_DEFAULT_HQ_TRANS_MS;
		else
			gain_params->fade_sg_length = frames * GAIN_DEFAULT_LQ_TRANS_MS;
	} else if (fade_period == GAIN_ZERO_TRANS_MS) {
		/* Special case for GAIN_ZERO_TRANS_MS to support zero fade-in transition time */
		gain_params->fade_sg_length = 0;
		return 0;
	} else {
		gain_params->fade_sg_length = frames * fade_period;
	}

	/* High precision step for fade-in calculation, keeps accurate precision */
	gain_params->step_i64 = INT64_MAX / gain_params->fade_sg_length;
	step_i64_to_i16 = gain_params->step_i64 >> I64_TO_I16_SHIFT;

	/* lower precision step for HIFI SIMD fade-in calculation, converted to Q16 format */
	gain_params->step_f16 = (MAX_GAIN_COEFFS_CNT / gain_params->channels_count) *
				step_i64_to_i16;

	/* Initialization gain for HIFI SIMD addition, depends on channel configuration */
	for (int i = 0; i < MAX_GAIN_COEFFS_CNT; i++) {
		gain_params->init_gain[i] = (i / gain_params->channels_count) *
					    step_i64_to_i16;
	}
	return 0;
}

int copier_gain_input16(struct comp_buffer *buff, enum copier_gain_state state,
			enum copier_gain_envelope_dir dir,
			struct copier_gain_params *gain_params, uint32_t frames)
{
	int16_t *dst = audio_stream_get_rptr(&buff->stream);
	const int nch = audio_stream_get_channels(&buff->stream);
	int samples = frames * nch;
	int16_t gain_env[MAX_GAIN_COEFFS_CNT] = {0};
	int16_t gain_env_sq;
	int16_t gain_env_i16;
	int16_t *dst_tmp;
	int16_t gain;
	int nmax, i, j;

	switch (state) {
	case STATIC_GAIN:
		/* static gain */
		if (gain_params->unity_gain)
			return 0;

		while (samples) {
			nmax = audio_stream_samples_without_wrap_s16(&buff->stream, dst);
			nmax = MIN(samples, nmax);

			for (j = 0; j < nch; j++) {
				dst_tmp = dst + j;
				gain = gain_params->gain_coeffs[j];
				for (i = 0; i < nmax; i += nch)
					dst_tmp[i] = q_multsr_sat_16x16(dst_tmp[i], gain,
									GAIN_Q10_INT_SHIFT);
			}
			samples -= nmax;
			dst = audio_stream_wrap(&buff->stream, dst + nmax);
		}
		break;
	case MUTE:
		while (samples) {
			nmax = audio_stream_samples_without_wrap_s16(&buff->stream, dst);
			nmax = MIN(samples, nmax);
			size_t zeroed_bytes = nmax * sizeof(int16_t);
			/* Apply mute */
			memset_s(dst, zeroed_bytes, 0, zeroed_bytes);
			samples -= nmax;
			dst = audio_stream_wrap(&buff->stream, dst + nmax);
		}
		break;
	case TRANS_GAIN:
		while (samples) {
			nmax = audio_stream_samples_without_wrap_s16(&buff->stream, dst);
			nmax = MIN(samples, nmax);

			/* Precalculate gain envelope */
			gain_env_i16 = gain_params->gain_env >> I64_TO_I16_SHIFT;
			for (i = 0; i < MAX_GAIN_COEFFS_CNT; i++)
				gain_env[i] = gain_env_i16 + gain_params->init_gain[i];

			/* Apply fade */
			for (j = 0; j < nch; j++) {
				dst += j;
				/* Quadratic fade part in Q15 format*/
				gain_env_sq = q_multsr_16x16(gain_env[j], gain_env[j], 15);

				/* Calculate gain value. Gain coeffs in Q10 format but
				 * gain_env_sq in Q15. So shifting result by 15 bits.
				 */
				gain = q_multsr_16x16(gain_params->gain_coeffs[j],
						      gain_env_sq, 15);

				for (i = 0; i < nmax; i += nch)
					dst[i] = q_multsr_sat_16x16(dst[i], gain,
								    GAIN_Q10_INT_SHIFT);
			}
			samples -= nmax;
			dst = audio_stream_wrap(&buff->stream, dst + nmax);
		}
		break;
	}

	if (state == MUTE) {
		gain_params->silence_sg_count += frames;
	} else if (state == TRANS_GAIN) {
		gain_params->fade_in_sg_count += frames;
		if (dir == GAIN_ADD)
			gain_params->gain_env += gain_params->step_i64 * frames;
		else
			gain_params->gain_env -= gain_params->step_i64 * frames;
	}

	return 0;
}

int copier_gain_input32(struct comp_buffer *buff, enum copier_gain_state state,
			enum copier_gain_envelope_dir dir,
			struct copier_gain_params *gain_params, uint32_t frames)
{
	int32_t *dst = audio_stream_get_rptr(&buff->stream);
	const int nch = audio_stream_get_channels(&buff->stream);
	int samples = frames * nch;
	int16_t gain_env[MAX_GAIN_COEFFS_CNT] = {0};
	int32_t *dst_tmp;
	int16_t gain, gain_env_i16, gain_env_sq;
	int nmax, i, j;

	switch (state) {
	case STATIC_GAIN:
		/* static gain */
		if (gain_params->unity_gain)
			return 0;

		while (samples) {
			nmax = audio_stream_samples_without_wrap_s32(&buff->stream, dst);
			nmax = MIN(samples, nmax);

			for (j = 0; j < nch; j++) {
				dst_tmp = dst + j;
				/* Gain is in Q21.10 format */
				gain = gain_params->gain_coeffs[j];
				for (i = 0; i < nmax; i += nch)
					dst_tmp[i] = q_multsr_sat_32x32(dst_tmp[i], gain,
									GAIN_Q10_INT_SHIFT);
			}
			samples -= nmax;
			dst = audio_stream_wrap(&buff->stream, dst + nmax);
		}
		break;
	case MUTE:
		while (samples) {
			nmax = audio_stream_samples_without_wrap_s32(&buff->stream, dst);
			nmax = MIN(samples, nmax);
			size_t zeroed_bytes = nmax * sizeof(int32_t);

			/* Apply mute*/
			memset_s(dst, zeroed_bytes, 0, zeroed_bytes);
			samples -= nmax;
			dst = audio_stream_wrap(&buff->stream, dst + nmax);
		}
		break;
	case TRANS_GAIN:
		while (samples) {
			nmax = audio_stream_samples_without_wrap_s32(&buff->stream, dst);
			nmax = MIN(samples, nmax);

			/* Precalculate gain envelope */
			gain_env_i16 = gain_params->gain_env >> I64_TO_I16_SHIFT;
			for (i = 0; i < MAX_GAIN_COEFFS_CNT; i++)
				gain_env[i] = gain_env_i16 + gain_params->init_gain[i];

			/* Apply fade */
			for (j = 0; j < nch; j++) {
				dst += j;
				/* Quadratic fade part in Q15 format*/
				gain_env_sq = q_multsr_16x16(gain_env[j], gain_env[j], 15);

				/* Calculate gain value. Gain coeffs in Q10 format but
				 * gain_env_sq in Q15. So shifting result by 15 bits.
				 */
				gain = q_multsr_16x16(gain_params->gain_coeffs[j],
						      gain_env_sq, 15);

				for (i = 0; i < nmax; i += nch)
					dst[i] =  q_multsr_sat_32x32(dst[i], gain,
								     GAIN_Q10_INT_SHIFT);
			}
			samples -= nmax;
			dst = audio_stream_wrap(&buff->stream, dst + nmax);
		}
		break;
	}

	if (state == MUTE) {
		gain_params->silence_sg_count += frames;
	} else if (state == TRANS_GAIN) {
		gain_params->fade_in_sg_count += frames;
		if (dir == GAIN_ADD)
			gain_params->gain_env += gain_params->step_i64 * frames;
		else
			gain_params->gain_env -= gain_params->step_i64 * frames;
	}

	return 0;
}

bool copier_is_unity_gain(struct copier_gain_params *gain_params)
{
	/* Set unity gain flag */
	for (int i = 0; i < MAX_GAIN_COEFFS_CNT; i++) {
		if (gain_params->gain_coeffs[i] != UNITY_GAIN_GENERIC)
			return false;
	}
	return true;
}

#endif

void copier_update_params(struct copier_data *cd, struct comp_dev *dev,
			  struct sof_ipc_stream_params *params)
{
	struct comp_buffer *sink;

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
	comp_dev_for_each_consumer(dev, sink) {
		int j;
		j = IPC4_SINK_QUEUE_ID(buf_get_id(sink));

		ipc4_update_buffer_format(sink, &cd->out_fmt[j]);
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

__cold int create_multi_endpoint_buffer(struct comp_dev *dev,
					struct copier_data *cd,
					const struct ipc4_copier_module_cfg *copier_cfg)
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

	assert_can_be_cold();

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
	buffer = buffer_new(NULL, &ipc_buf, false);
	if (!buffer)
		return -ENOMEM;

	audio_stream_set_channels(&buffer->stream, copier_cfg->base.audio_fmt.channels_count);
	audio_stream_set_rate(&buffer->stream, copier_cfg->base.audio_fmt.sampling_frequency);
	audio_stream_set_frm_fmt(&buffer->stream, config->frame_fmt);
	audio_stream_set_valid_fmt(&buffer->stream, valid_fmt);
	audio_stream_set_buffer_fmt(&buffer->stream,
				    copier_cfg->base.audio_fmt.interleaving_style);

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		audio_buffer_set_chmap(&buffer->audio_buffer, i, (chan_map >> i * 4) & 0xf);

	audio_buffer_set_hw_params_configured(&buffer->audio_buffer);

	cd->multi_endpoint_buffer = buffer;

	return 0;
}

__cold enum sof_ipc_stream_direction
	get_gateway_direction(enum ipc4_connector_node_id_type node_id_type)
{
	assert_can_be_cold();

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

static bool is_remapping_chmap(uint32_t chmap, size_t out_channel_count)
{
	size_t i;

	assert(out_channel_count <= 8);

	for (i = 0; i < out_channel_count; i++) {
		if ((chmap & 0xf) != i)
			return true;
		chmap >>= 4;
	}

	return false;
}

pcm_converter_func get_converter_func(const struct ipc4_audio_format *in_fmt,
				      const struct ipc4_audio_format *out_fmt,
				      enum ipc4_gateway_type type,
				      enum ipc4_direction_type dir,
				      uint32_t chmap)
{
	enum sof_ipc_frame in, in_valid, out, out_valid;

	audio_stream_fmt_conversion(in_fmt->depth, in_fmt->valid_bit_depth, &in, &in_valid,
				    in_fmt->s_type);
	audio_stream_fmt_conversion(out_fmt->depth, out_fmt->valid_bit_depth, &out, &out_valid,
				    out_fmt->s_type);

	/* use MSB sample type to select conversion function if the data is enter or exit dsp.
	 * In playback case, host input and dai output and in capture case, host output and
	 * dai input.
	 */
	if (in_fmt->s_type == IPC4_TYPE_MSB_INTEGER && in_valid == SOF_IPC_FRAME_S24_4LE) {
		switch (type) {
		case ipc4_gtw_host:
			if (dir == ipc4_playback)
				in_valid = SOF_IPC_FRAME_S24_4LE_MSB;
			break;
		case ipc4_gtw_alh:
		case ipc4_gtw_link:
		case ipc4_gtw_ssp:
		case ipc4_gtw_dmic:
			if (dir == ipc4_capture)
				in_valid = SOF_IPC_FRAME_S24_4LE_MSB;
			break;
		default:
			break;
		}
	}

	if (out_fmt->s_type == IPC4_TYPE_MSB_INTEGER && out_valid == SOF_IPC_FRAME_S24_4LE) {
		switch (type) {
		case ipc4_gtw_host:
			if (dir == ipc4_capture)
				out_valid = SOF_IPC_FRAME_S24_4LE_MSB;
			break;
		case ipc4_gtw_alh:
		case ipc4_gtw_link:
		case ipc4_gtw_ssp:
		case ipc4_gtw_dmic:
			if (dir == ipc4_playback)
				out_valid = SOF_IPC_FRAME_S24_4LE_MSB;
			break;
		default:
			break;
		}
	}

	if (in_fmt->channels_count != out_fmt->channels_count ||
	    is_remapping_chmap(chmap, out_fmt->channels_count)) {
		if (in_valid == SOF_IPC_FRAME_S16_LE && in == SOF_IPC_FRAME_S32_LE)
			in = SOF_IPC_FRAME_S16_4LE;
		if (out_valid == SOF_IPC_FRAME_S16_LE && out == SOF_IPC_FRAME_S32_LE)
			out = SOF_IPC_FRAME_S16_4LE;

		return pcm_get_remap_function(in, out);
	}

	/* check container & sample size */
	if (use_no_container_convert_function(in, in_valid, out, out_valid))
		return pcm_get_conversion_function(in, out);
	else
		return pcm_get_conversion_vc_function(in, in_valid, out, out_valid, type, dir);
}
