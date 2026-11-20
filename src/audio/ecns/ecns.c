/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

/**
 * \file
 * \brief ECNS (Echo Cancellation & Noise Suppression) DP component
 *
 * Input pin 0 receives the 16 kHz microphone stream and input pin 1 the
 * 16 kHz echo reference. The microphone channels are averaged to mono, or a
 * single channel is selected with the microphone channel enum control, and
 * high-pass filtered. When the AEC switch is enabled the average of the echo
 * reference channels is subtracted from the filtered microphone signal. The
 * result is written as 16 kHz mono s16 or s32 to the single output pin. The
 * echo reference is always consumed, also when the AEC switch is disabled.
 */

#include <sof/audio/component_ext.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/ecns.h>
#include <sof/common.h>
#include <sof/lib/memory.h>
#include <sof/math/iir_df1.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc4/base_fw.h>
#include <ipc4/header.h>
#include <ipc/stream.h>
#include <ipc4/base-config.h>
#include <user/eq.h>
#include <rtos/init.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ecns, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(ecns);
DECLARE_TR_CTX(ecns_tr, SOF_UUID(ecns_uuid), LOG_LEVEL_INFO);

/**
 * \brief Get the microphone channels used for the mono output.
 *
 * The selection comes from the microphone channel enum control. Value 0
 * averages all channels, values 1..N select a single channel. A channel
 * beyond the input channel count falls back to the average of all channels.
 *
 * \param[in]  mic_sel Microphone channel enum control value
 * \param[in]  ch      Number of microphone input channels
 * \param[out] start   First channel index to average
 * \param[out] cnt     Number of channels to average
 */
static inline void ecns_mono_window(uint32_t mic_sel, uint32_t ch, uint32_t *start,
				    uint32_t *cnt)
{
	if (mic_sel != ECNS_MIC_SEL_AVERAGE && mic_sel <= ch) {
		*start = mic_sel - 1;
		*cnt = 1;
	} else {
		*start = 0;
		*cnt = ch ? ch : 1;
	}
}

/**
 * \brief Average channels of one interleaved frame into a Q1.31 sample.
 *
 * \param[in] frame Pointer to the first sample of the frame
 * \param[in] s32   True for 32-bit samples, false for 16-bit samples
 * \param[in] start First channel index to average
 * \param[in] cnt   Number of channels to average, must be non-zero
 * \return Average of the channels as Q1.31
 */
static inline int32_t ecns_frame_avg(const void *frame, bool s32, uint32_t start, uint32_t cnt)
{
	int64_t acc = 0;
	uint32_t c;

	if (s32) {
		const int32_t *x = (const int32_t *)frame + start;

		for (c = 0; c < cnt; c++)
			acc += x[c];
	} else {
		const int16_t *x = (const int16_t *)frame + start;

		for (c = 0; c < cnt; c++)
			acc += (int32_t)x[c] << 16;
	}

	return (int32_t)(acc / (int32_t)cnt);
}

#if CONFIG_ECNS_KPB_HPF
/**
 * \brief 100 Hz second-order high-pass filter with +30 dB gain at 16 kHz.
 *
 * Converted from tools/topology/topology2/include/components/eqiir/highpass_100hz_30db_16khz.conf
 * consisting of struct sof_eq_iir_header (6 words) followed by one
 * struct sof_eq_iir_biquad (7 words):
 *   header: num_sections=1, num_sections_in_series=1, reserved[4]={0,0,0,0}
 *   biquad: a2, a1, b2, b1, b0 in Q2.30, shift=-5, gain=0x7e33 (32307 in Q2.14)
 */
static const uint32_t hp_100hz_30db_16k[13] = {
	0x00000001, 0x00000001, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0xc37518f5, 0x7c72526b,
	0x1f32926e, 0xc19adb24, 0x1f32926e, 0xfffffffb,
	0x00007e33
};

/** \brief As hp_100hz_30db_16k with 0 dB gain, from highpass_100hz_0db_16khz.conf. */
static const uint32_t hp_100hz_0db_16k[13] = {
	0x00000001, 0x00000001, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0xc37518f5, 0x7c72526b,
	0x1f32926e, 0xc19adb24, 0x1f32926e, 0x00000000,
	0x00007fb4
};
#endif

/** \brief Switch control indexes, mixer."1" to mixer."3" in ecns.conf */
#define ECNS_CTRL_INDEX_CAPTURE_BOOST	0
#define ECNS_CTRL_INDEX_AEC		1
#define ECNS_CTRL_INDEX_NS		2

/** \brief Enum control indexes, enum."1" in the ECNS widget instance */
#define ECNS_CTRL_INDEX_MIC_SEL		0

/** \brief ECNS component private data */
struct ecns_comp_data {
	struct ipc4_base_module_cfg base_cfg;	/**< IPC4 base config, pin 0 input format */
	bool capture_boost;		/**< Boosted high-pass filter response in use */
	bool capture_boost_req;		/**< Boost requested by the control */
	bool aec_enable;		/**< AEC switch, subtract the echo reference */
	bool ns_enable;			/**< NS switch, no function yet */
	uint32_t mic_sel;		/**< Microphone channel, 0 average, 1..N single channel */
	struct iir_state_df1 hp_iir;	/**< Output high-pass filter state */
	int32_t *hp_delay;		/**< Output high-pass filter delay line */
};

#if CONFIG_ECNS_KPB_HPF
static struct sof_eq_iir_header *ecns_iir_coef(struct ecns_comp_data *cd)
{
	return (struct sof_eq_iir_header *)
		(cd->capture_boost ? hp_100hz_30db_16k : hp_100hz_0db_16k);
}
#endif

/* Only the coefficient pointers move, the filter delay line is kept. */
static void ecns_update_iir_coef(struct ecns_comp_data *cd)
{
#if CONFIG_ECNS_KPB_HPF
	iir_init_coef_df1(&cd->hp_iir, ecns_iir_coef(cd));
#endif
}

static struct comp_dev *ecns_new(const struct comp_driver *drv,
				 const struct comp_ipc_config *config,
				 const void *spec)
{
	const struct ipc4_base_module_cfg *base_cfg = spec;
	struct ecns_comp_data *cd;
	struct comp_dev *dev;

	comp_cl_info(drv, "ecns_new");

	if (!base_cfg) {
		comp_cl_err(drv, "ecns_new: no base config");
		return NULL;
	}

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	cd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*cd));
	if (!cd) {
		comp_free_device(dev);
		return NULL;
	}

	memcpy_s(&cd->base_cfg, sizeof(cd->base_cfg), base_cfg, sizeof(*base_cfg));

	comp_set_drvdata(dev, cd);
	dev->direction     = SOF_IPC_STREAM_CAPTURE;
	dev->direction_set = true;
	dev->state         = COMP_STATE_READY;

	/* Boosted until the "ECNS capture boost" control says otherwise */
	cd->capture_boost = true;
	cd->capture_boost_req = true;

#if CONFIG_ECNS_KPB_HPF
	struct sof_eq_iir_header *hp_hdr = ecns_iir_coef(cd);
	int hp_size = iir_delay_size_df1(hp_hdr);
	int32_t *delay;

	if (hp_size < 0) {
		rfree(cd);
		comp_free_device(dev);
		return NULL;
	}

	cd->hp_delay = rzalloc(SOF_MEM_FLAG_USER, hp_size);
	if (!cd->hp_delay) {
		rfree(cd);
		comp_free_device(dev);
		return NULL;
	}

	delay = cd->hp_delay;
	iir_init_coef_df1(&cd->hp_iir, hp_hdr);
	iir_init_delay_df1(&cd->hp_iir, &delay);
#endif

	comp_info(dev, "ecns_new: rate=%u in_ch=%u valid_bits=%u",
		  base_cfg->audio_fmt.sampling_frequency,
		  base_cfg->audio_fmt.channels_count,
		  base_cfg->audio_fmt.valid_bit_depth);

	return dev;
}

static void ecns_free(struct comp_dev *dev)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "ecns_free");
	rfree(cd->hp_delay);
	rfree(cd);
	comp_free_device(dev);
}

static int ecns_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "ecns_params");

	ipc4_base_module_cfg_to_stream_params(&cd->base_cfg, params);
	component_set_nearest_period_frames(dev, params->rate);

	return 0;
}

/**
 * \brief Get the output sample container depth.
 *
 * The output format is defined in topology, but the IPC4 base config of ECNS
 * only describes the input pin 0 format. The topology output format matches
 * the consumer input format, so the depth is taken from the consumer base
 * config.
 *
 * \param[in] dev  ECNS component device
 * \param[in] sink Output buffer
 * \return 16 or 32
 */
static uint32_t ecns_out_depth(struct comp_dev *dev, struct comp_buffer *sink)
{
	struct comp_dev *consumer = comp_buffer_get_sink_component(sink);
	struct ipc4_base_module_cfg cfg;

	if (consumer && !comp_get_attribute(consumer, COMP_ATTR_BASE_CONFIG, &cfg) &&
	    cfg.audio_fmt.depth == 16)
		return 16;

	return 32;
}

static int ecns_prepare(struct comp_dev *dev)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink;

	comp_info(dev, "ecns_prepare");

	comp_dev_for_each_consumer(dev, sink) {
		if (IPC4_SRC_QUEUE_ID(buf_get_id(sink)) != ECNS_PIN_OUT)
			continue;

		uint32_t depth = ecns_out_depth(dev, sink);
		struct ipc4_audio_format fmt = {
			.sampling_frequency = ECNS_RATE,
			.channels_count = 1,
			.depth = depth,
			.valid_bit_depth = depth,
			.s_type = depth == 16 ? IPC4_TYPE_SIGNED_INTEGER : IPC4_TYPE_MSB_INTEGER,
			.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
			.ch_map = ECNS_CH_MAP_MONO,
			.ch_cfg = IPC4_CHANNEL_CONFIG_MONO,
		};

		ipc4_update_buffer_format(sink, &fmt);
		comp_info(dev, "ecns_prepare: output s%u", depth);
	}

	cd->capture_boost = cd->capture_boost_req;
	ecns_update_iir_coef(cd);

	return comp_set_state(dev, COMP_TRIGGER_PREPARE);
}

static int ecns_reset(struct comp_dev *dev)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "ecns_reset");
#if CONFIG_ECNS_KPB_HPF
	if (cd->hp_delay) {
		int hp_size = iir_delay_size_df1(ecns_iir_coef(cd));

		if (hp_size > 0)
			memset_s(cd->hp_delay, hp_size, 0, hp_size);
	}
#endif
	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int ecns_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "ecns_trigger cmd=%d", cmd);
	return comp_set_state(dev, cmd);
}

static inline int32_t ecns_filter(struct ecns_comp_data *cd, int32_t x)
{
#if CONFIG_ECNS_KPB_HPF
	return iir_df1(&cd->hp_iir, x);
#else
	return x;
#endif
}

/** \brief Stream properties used by the processing loop */
struct ecns_stream {
	struct comp_buffer *buf;	/**< Buffer, NULL if the pin is not connected */
	uint32_t ch;			/**< Number of channels */
	uint32_t frame_bytes;		/**< Bytes per frame */
	bool s32;			/**< True for 32-bit, false for 16-bit samples */
};

static void ecns_stream_init(struct ecns_stream *s, struct comp_buffer *buf)
{
	uint32_t sample_bytes;

	s->buf = buf;
	if (!buf)
		return;

	s->ch = audio_stream_get_channels(&buf->stream);
	if (!s->ch)
		s->ch = 1;

	sample_bytes = audio_stream_sample_bytes(&buf->stream);
	s->s32 = sample_bytes != sizeof(int16_t);
	s->frame_bytes = s->ch * (s->s32 ? sizeof(int32_t) : sizeof(int16_t));
}

static uint32_t ecns_avail_frames(const struct ecns_stream *s)
{
	return s->buf ? audio_stream_get_avail_bytes(&s->buf->stream) / s->frame_bytes : 0;
}

/**
 * \brief Process microphone and echo reference frames to the output.
 *
 * \param[in,out] cd         ECNS component data
 * \param[in]     mic        Microphone input stream
 * \param[in]     ref        Echo reference input stream, buffer may be NULL
 * \param[in,out] out        Output stream
 * \param[in]     frames     Number of frames to process
 * \param[in]     ref_frames Number of available echo reference frames. The
 *                           frames after these are processed as if the
 *                           reference was zero.
 */
static void ecns_process(struct ecns_comp_data *cd, const struct ecns_stream *mic,
			 const struct ecns_stream *ref, const struct ecns_stream *out,
			 uint32_t frames, uint32_t ref_frames)
{
	uint8_t *mic_ptr = audio_stream_get_rptr(&mic->buf->stream);
	uint8_t *out_ptr = audio_stream_get_wptr(&out->buf->stream);
	uint8_t *ref_ptr = ref_frames ? audio_stream_get_rptr(&ref->buf->stream) : NULL;
	uint32_t mic_sample_bytes = mic->s32 ? sizeof(int32_t) : sizeof(int16_t);
	uint32_t avg_start, avg_cnt;
	uint32_t remaining = frames;
	uint32_t i, n;
	int32_t y;

	ecns_mono_window(cd->mic_sel, mic->ch, &avg_start, &avg_cnt);

	while (remaining > 0) {
		n = MIN(remaining, audio_stream_frames_without_wrap(&mic->buf->stream, mic_ptr));
		n = MIN(n, audio_stream_frames_without_wrap(&out->buf->stream, out_ptr));
		if (ref_frames) {
			n = MIN(n, ref_frames);
			n = MIN(n, audio_stream_frames_without_wrap(&ref->buf->stream, ref_ptr));
		}

		for (i = 0; i < n; i++) {
			y = ecns_filter(cd, ecns_frame_avg(mic_ptr, mic->s32, avg_start, avg_cnt));
			mic_ptr += mic->ch * mic_sample_bytes;

			if (ref_frames) {
				if (cd->aec_enable)
					y = sat_int32((int64_t)y -
						      ecns_frame_avg(ref_ptr, ref->s32, 0, ref->ch));
				ref_ptr += ref->frame_bytes;
			}

			if (out->s32) {
				int32_t *o = (int32_t *)out_ptr;

				o[0] = y;
				for (uint32_t c = 1; c < out->ch; c++)
					o[c] = 0;
			} else {
				int16_t *o = (int16_t *)out_ptr;

				o[0] = sat_int16(Q_SHIFT_RND(y, 31, 15));
				for (uint32_t c = 1; c < out->ch; c++)
					o[c] = 0;
			}
			out_ptr += out->frame_bytes;
		}

		remaining -= n;
		mic_ptr = audio_stream_wrap(&mic->buf->stream, mic_ptr);
		out_ptr = audio_stream_wrap(&out->buf->stream, out_ptr);
		if (ref_frames) {
			ref_frames -= n;
			ref_ptr = ref_frames ? audio_stream_wrap(&ref->buf->stream, ref_ptr) : NULL;
		}
	}
}

/** \brief Drop all complete frames from an input stream. */
static void ecns_drain(const struct ecns_stream *s)
{
	uint32_t frames = ecns_avail_frames(s);

	if (frames)
		comp_update_buffer_consume(s->buf, frames * s->frame_bytes);
}

/*
 * Main 20 ms DP processing function:
 * Consumes:
 *   - Pin 0 in: 16 kHz microphone stream (2 or 4 channels, 16/32-bit)
 *   - Pin 1 in: 16 kHz echo reference (stereo, 16/32-bit)
 * Produces:
 *   - Pin 0 out: 16 kHz mono processed stream (16/32-bit)
 */
static int ecns_copy(struct comp_dev *dev)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *src_mic = NULL;
	struct comp_buffer *src_ref = NULL;
	struct comp_buffer *snk = NULL;
	struct comp_buffer *source;
	struct comp_buffer *sink;
	struct ecns_stream mic, ref, out;
	uint32_t frames, ref_frames;

	if (list_is_empty(&dev->bsource_list))
		return 0;

	/* Apply a capture boost change from the control in the processing context */
	if (cd->capture_boost != cd->capture_boost_req) {
		cd->capture_boost = cd->capture_boost_req;
		ecns_update_iir_coef(cd);
		comp_info(dev, "capture boost %s",
			  cd->capture_boost ? "enabled" : "disabled");
	}

	comp_dev_for_each_producer(dev, source) {
		uint32_t pin = IPC4_SINK_QUEUE_ID(buf_get_id(source));

		if (pin == ECNS_PIN_MIC_IN && !src_mic)
			src_mic = source;
		else if (pin == ECNS_PIN_REF_IN && !src_ref)
			src_ref = source;
	}

	comp_dev_for_each_consumer(dev, sink) {
		if (IPC4_SRC_QUEUE_ID(buf_get_id(sink)) == ECNS_PIN_OUT && !snk)
			snk = sink;
	}

	ecns_stream_init(&mic, src_mic);
	ecns_stream_init(&ref, src_ref);
	ecns_stream_init(&out, snk);

	/* Drain all available input bounded by sink space; a one-period cap
	 * would let the source ring overflow under DMIC vs DSP clock drift.
	 */
	frames = ecns_avail_frames(&mic);
	if (snk && comp_buffer_get_sink_state(snk) == COMP_STATE_ACTIVE)
		frames = MIN(frames, audio_stream_get_free_bytes(&snk->stream) / out.frame_bytes);
	else
		frames = 0;

	if (!frames) {
		/* Drop the input when the output can't take it to prevent
		 * upstream DAI FIFO overruns.
		 */
		if (!snk || comp_buffer_get_sink_state(snk) != COMP_STATE_ACTIVE ||
		    !audio_stream_get_free_bytes(&snk->stream)) {
			if (src_mic)
				ecns_drain(&mic);
			if (src_ref)
				ecns_drain(&ref);
		}
		return 0;
	}

	/* A missing echo reference is processed as zero */
	ref_frames = MIN(frames, ecns_avail_frames(&ref));

	buffer_stream_invalidate(src_mic, frames * mic.frame_bytes);
	if (ref_frames)
		buffer_stream_invalidate(src_ref, ref_frames * ref.frame_bytes);

	ecns_process(cd, &mic, &ref, &out, frames, ref_frames);

	buffer_stream_writeback(snk, frames * out.frame_bytes);
	comp_update_buffer_produce(snk, frames * out.frame_bytes);
	comp_update_buffer_consume(src_mic, frames * mic.frame_bytes);
	if (ref_frames)
		comp_update_buffer_consume(src_ref, ref_frames * ref.frame_bytes);

	return 0;
}

static int ecns_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);

	if (type == COMP_ATTR_BASE_CONFIG) {
		*(struct ipc4_base_module_cfg *)value = cd->base_cfg;
		return 0;
	}
	return -EINVAL;
}

static int ecns_set_large_config(struct comp_dev *dev,
				 uint32_t param_id,
				 bool first_block,
				 bool last_block,
				 uint32_t data_offset,
				 const char *data)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID: {
		const struct sof_ipc4_control_msg_payload *cp =
			(const struct sof_ipc4_control_msg_payload *)data;
		bool val;

		if (cp->num_elems < 1)
			return -EINVAL;

		val = !!cp->chanv[0].value;
		switch (cp->id) {
		case ECNS_CTRL_INDEX_CAPTURE_BOOST:
			cd->capture_boost_req = val;
			return 0;
		case ECNS_CTRL_INDEX_AEC:
			cd->aec_enable = val;
			comp_info(dev, "AEC %s", val ? "enabled" : "disabled");
			return 0;
		case ECNS_CTRL_INDEX_NS:
			cd->ns_enable = val;
			comp_info(dev, "NS %s", val ? "enabled" : "disabled");
			return 0;
		default:
			comp_err(dev, "illegal switch control id %u", cp->id);
			return -EINVAL;
		}
	}
	case SOF_IPC4_ENUM_CONTROL_PARAM_ID: {
		const struct sof_ipc4_control_msg_payload *cp =
			(const struct sof_ipc4_control_msg_payload *)data;

		if (cp->num_elems < 1)
			return -EINVAL;

		if (cp->id != ECNS_CTRL_INDEX_MIC_SEL) {
			comp_err(dev, "illegal enum control id %u", cp->id);
			return -EINVAL;
		}

		if (cp->chanv[0].value > ECNS_MIC_SEL_MAX) {
			comp_err(dev, "illegal mic channel %u", cp->chanv[0].value);
			return -EINVAL;
		}

		cd->mic_sel = cp->chanv[0].value;
		comp_info(dev, "mic channel %u", cd->mic_sel);
		return 0;
	}
	default:
		return -EINVAL;
	}
}

static int ecns_get_large_config(struct comp_dev *dev,
				 uint32_t param_id,
				 bool first_block,
				 bool last_block,
				 uint32_t *data_offset,
				 char *data)
{
	struct ecns_comp_data *cd = comp_get_drvdata(dev);

	struct sof_ipc4_control_msg_payload *cp = (struct sof_ipc4_control_msg_payload *)data;
	uint32_t resp_size = sizeof(struct sof_ipc4_control_msg_payload) +
			     sizeof(struct sof_ipc4_ctrl_value_chan);
	uint16_t ctl_id;
	uint32_t val;

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
		ctl_id = cp->id;
		switch (ctl_id) {
		case ECNS_CTRL_INDEX_CAPTURE_BOOST:
			val = cd->capture_boost_req;
			break;
		case ECNS_CTRL_INDEX_AEC:
			val = cd->aec_enable;
			break;
		case ECNS_CTRL_INDEX_NS:
			val = cd->ns_enable;
			break;
		default:
			comp_err(dev, "illegal switch control id %u", ctl_id);
			return -EINVAL;
		}
		break;
	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		ctl_id = cp->id;
		if (ctl_id != ECNS_CTRL_INDEX_MIC_SEL) {
			comp_err(dev, "illegal enum control id %u", ctl_id);
			return -EINVAL;
		}
		val = cd->mic_sel;
		break;
	default:
		return -EINVAL;
	}

	if (resp_size > *data_offset) {
		comp_err(dev, "wrong control response size %u vs %u", resp_size, *data_offset);
		return -EINVAL;
	}

	*data_offset = resp_size;
	memset_s(cp, resp_size, 0, resp_size);
	cp->id = ctl_id;
	cp->num_elems = 1;
	cp->chanv[0].channel = 0;
	cp->chanv[0].value = val;
	return 0;
}

static const struct comp_driver ecns_drv = {
	.type	= SOF_COMP_NONE,
	.uid	= SOF_RT_UUID(ecns_uuid),
	.tctx	= &ecns_tr,
	.ops	= {
		.create		= ecns_new,
		.free		= ecns_free,
		.params		= ecns_params,
		.prepare	= ecns_prepare,
		.reset		= ecns_reset,
		.trigger	= ecns_trigger,
		.copy		= ecns_copy,
		.set_large_config = ecns_set_large_config,
		.get_large_config = ecns_get_large_config,
		.get_attribute	= ecns_get_attribute,
	},
};

static struct comp_driver_info ecns_info = {
	.drv = &ecns_drv,
};

UT_STATIC void sys_comp_ecns_init(void)
{
	comp_register(&ecns_info);
}

DECLARE_MODULE(sys_comp_ecns_init);
SOF_MODULE_INIT(ecns, sys_comp_ecns_init);
