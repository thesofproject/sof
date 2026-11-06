// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// FFmpeg (libavcodec) audio decoder wrapper - SOF module core.
//
// This translation unit contains the SOF module_interface glue only; the actual
// decoding is delegated to the backend selected at build time (see
// ffmpeg_dec-stub.c and ffmpeg_dec-ffmpeg.c). The input is a compressed audio
// elementary stream (raw data), the output is interleaved PCM.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/source_api.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <errno.h>
#include "ffmpeg_dec.h"

#include <ipc/compress_params.h>
#if CONFIG_IPC_MAJOR_4
#include <sof/audio/audio_buffer.h>
#include <sof/ipc/msg.h>
#include <ipc4/module.h>
#include <ipc4/notification.h>
#endif

/* UUID identifies the component. Registered in uuid-registry.txt. */
SOF_DEFINE_REG_UUID(ffmpeg_dec);

/* Creates logging data for the component */
LOG_MODULE_REGISTER(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_IPC_MAJOR_4
/*
 * Build the module notification the driver expects to unblock compress_drain().
 * The kernel's compress EOS handler (sof_ipc4_compr_drain_done) matches on the
 * COMPR "magic" event id, so use the same template the Cadence decoder does.
 */
__cold struct ipc_msg *ffmpeg_dec_eos_notification_init(struct processing_module *mod)
{
	struct comp_ipc_config *ipc_config = &mod->dev->ipc_config;
	struct sof_ipc4_notify_module_data *msg_data;
	union ipc4_notification_header primary;
	struct ipc_msg *msg;

	primary.dat = 0;
	primary.r.notif_type = SOF_IPC4_MODULE_NOTIFICATION;
	primary.r.type = SOF_IPC4_GLB_NOTIFICATION;
	primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;

	msg = mod_ipc_msg_w_ext_init(mod, primary.dat, 0, sizeof(*msg_data));
	if (!msg)
		return NULL;

	msg_data = (struct sof_ipc4_notify_module_data *)msg->tx_data;
	msg_data->instance_id = IPC4_INST_ID(ipc_config->id);
	msg_data->module_id = IPC4_MOD_ID(ipc_config->id);
	msg_data->event_id = SOF_IPC4_NOTIFY_MODULE_EVENTID_COMPR_MAGIC_VAL;
	msg_data->event_data_size = 0;

	return msg;
}

/*
 * Report that the compress drain is complete: send the EOS notification (which
 * unblocks the host's compress_drain()) exactly once, and mark the sink so the
 * end-of-stream state propagates downstream to the DAI.
 */
void ffmpeg_dec_signal_eos(struct processing_module *mod, struct sof_sink *sink)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);

	if (cd->eos_sent || !cd->eos_msg)
		return;

	ipc_msg_send(cd->eos_msg, NULL, false);
	cd->eos_sent = true;
	audio_buffer_set_eos(sof_audio_buffer_from_sink(sink));
	comp_info(mod->dev, "EOS: compress drain complete");
}
#endif /* CONFIG_IPC_MAJOR_4 */

/* Decoder-mode ops (compressed -> PCM). */
int ffmpeg_dec_store_extradata(struct processing_module *mod,
			       const uint8_t *data, size_t size)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	if (!size)
		return 0;

	if (size > FFMPEG_DEC_MAX_EXTRADATA) {
		comp_err(dev, "extradata too large: %zu", size);
		return -EINVAL;
	}

	memcpy_s(cd->extradata, sizeof(cd->extradata), data, size);
	cd->extradata_size = size;
	comp_info(dev, "stored %zu bytes of codec extradata", size);
	return 0;
}

static inline size_t ffmpeg_dec_min(size_t a, size_t b)
{
	return a < b ? a : b;
}

/* Copy @bytes out of a circular source buffer (starting at @src) into the
 * linear @dst, wrapping at the buffer end if necessary.
 */
void ffmpeg_dec_copy_from_circular(void *dst, const void *src,
				   const void *buf_start, size_t buf_size,
				   size_t bytes)
{
	size_t to_end = (size_t)((const uint8_t *)buf_start + buf_size -
				 (const uint8_t *)src);

	if (to_end >= bytes) {
		memcpy_s(dst, bytes, src, bytes);
		return;
	}
	memcpy_s(dst, to_end, src, to_end);
	memcpy_s((uint8_t *)dst + to_end, bytes - to_end,
		 buf_start, bytes - to_end);
}

/* Copy @bytes from the linear @src into a circular sink buffer (starting at
 * @dst), wrapping at the buffer end if necessary.
 */
void ffmpeg_dec_copy_to_circular(void *dst, void *buf_start,
				 size_t buf_size, const void *src,
				 size_t bytes)
{
	size_t to_end = (size_t)((uint8_t *)buf_start + buf_size -
				 (uint8_t *)dst);

	if (to_end >= bytes) {
		memcpy_s(dst, bytes, src, bytes);
		return;
	}
	memcpy_s(dst, to_end, src, to_end);
	memcpy_s(buf_start, bytes - to_end,
		 (const uint8_t *)src + to_end, bytes - to_end);
}

/*
 * Peek at the start of the stream and skip a leading ID3v2 tag if present.
 * Returns:
 *   1 if an ID3v2 tag was completely skipped (caller should re-check),
 *   0 if the stream does not start with an ID3v2 tag (cd->in_buf holds the
 *     peeked header bytes),
 *  -ENODATA while waiting for enough bytes to inspect or finish skipping.
 */
int ffmpeg_dec_skip_id3v2(struct processing_module *mod, struct sof_source *src)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	const void *sp, *sstart;
	size_t sbytes, avail, req;
	int ret;

	/* Drain any pending skip bytes from a previous chunk */
	if (cd->skip_bytes > 0) {
		avail = source_get_data_available(src);
		if (!avail)
			return -ENODATA;
		size_t to_skip = ffmpeg_dec_min(avail, cd->skip_bytes);

		source_drop_data(src, to_skip);
		cd->skip_bytes -= to_skip;
		if (cd->skip_bytes > 0)
			return -ENODATA;
	}

	avail = source_get_data_available(src);
	if (avail < 4)
		return -ENODATA;

	req = ffmpeg_dec_min(avail, cd->in_buf_size);
	ret = source_get_data(src, req, &sp, &sstart, &sbytes);
	if (ret)
		return ret;
	ffmpeg_dec_copy_from_circular(cd->in_buf, sp, sstart, sbytes, req);
	source_release_data(src, 0);

	/* ID3v2 tag: 10-byte header with 28-bit syncsafe length */
	if (memcmp(cd->in_buf, "ID3", 3) == 0) {
		uint32_t tag;
		size_t total_tag;

		if (req < 10)
			return -ENODATA;

		tag = ((uint32_t)(cd->in_buf[6] & 0x7f) << 21) |
		      ((uint32_t)(cd->in_buf[7] & 0x7f) << 14) |
		      ((uint32_t)(cd->in_buf[8] & 0x7f) << 7) |
		      (uint32_t)(cd->in_buf[9] & 0x7f);
		total_tag = 10 + tag + ((cd->in_buf[5] & 0x10) ? 10 : 0);
		comp_info(dev, "ID3v2 tag detected: %zu bytes", total_tag);

		if (avail >= total_tag) {
			source_drop_data(src, total_tag);
			return 1;
		}

		source_drop_data(src, avail);
		cd->skip_bytes = total_tag - avail;
		return -ENODATA;
	}

	return 0;
}

static const struct ffmpeg_dec_codec_ops * const ffmpeg_dec_codecs[] = {
#if defined(CONFIG_FFMPEG_DEC_FLAC) || defined(CONFIG_COMP_FFMPEG_DEC_STUB)
	&ffmpeg_dec_flac_ops,
#endif
#if defined(CONFIG_FFMPEG_DEC_MP3) || defined(CONFIG_COMP_FFMPEG_DEC_STUB)
	&ffmpeg_dec_mp3_ops,
#endif
#if defined(CONFIG_FFMPEG_DEC_AAC) || defined(CONFIG_COMP_FFMPEG_DEC_STUB)
	&ffmpeg_dec_aac_ops,
#endif
#if defined(CONFIG_FFMPEG_DEC_OPUS) || defined(CONFIG_COMP_FFMPEG_DEC_STUB)
	&ffmpeg_dec_opus_ops,
#endif
};

static const struct ffmpeg_dec_codec_ops *ffmpeg_dec_find_codec_ops(enum ffmpeg_dec_codec id)
{
	for (size_t i = 0; i < ARRAY_SIZE(ffmpeg_dec_codecs); i++) {
		if (ffmpeg_dec_codecs[i]->id == id)
			return ffmpeg_dec_codecs[i];
	}
	return NULL;
}

#if CONFIG_IPC_MAJOR_4
static const struct ffmpeg_dec_codec_ops *ffmpeg_dec_find_codec_ops_by_snd_id(uint32_t snd_id)
{
	if (snd_id == SND_AUDIOCODEC_BESPOKE)
		snd_id = SND_AUDIOCODEC_OPUS_RAW;

	for (size_t i = 0; i < ARRAY_SIZE(ffmpeg_dec_codecs); i++) {
		if (ffmpeg_dec_codecs[i]->snd_codec_id == snd_id)
			return ffmpeg_dec_codecs[i];
	}
	return NULL;
}
#endif

/*
 * Largest number of PCM samples-per-channel one decoded frame of @cd can
 * yield. Used only to size the DP output ring buffer and PCM staging buffer;
 * a generous value is harmless, an undersized one throttles playback.
 */
static uint32_t ffmpeg_dec_max_frame_samples(const struct ffmpeg_dec_comp_data *cd)
{
	if (cd->codec_ops && cd->codec_ops->max_frame_samples)
		return cd->codec_ops->max_frame_samples(cd);

	return 2048;
}

/**
 * ffmpeg_dec_init() - Initialize the ffmpeg_dec component.
 * @mod: Pointer to module data.
 *
 * Allocates private data and hands off to the selected decode or encode backend
 * for its one-time initialization. __cold marks this non-critical path for
 * slower DRAM.
 *
 * Return: Zero if success, otherwise error code.
 */
__cold static int ffmpeg_dec_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_dec_comp_data *cd;
	uint32_t frame_bytes;
	int ret;

	comp_info(dev, "entry");

#if CONFIG_IPC_MAJOR_4
	if (mod->priv.cfg.ext_data &&
	    mod->priv.cfg.ext_data->module_data &&
	    mod->priv.cfg.ext_data->module_data_size >= sizeof(struct snd_codec)) {
		const uint8_t *init_bytes = mod->priv.cfg.ext_data->module_data;
		uint32_t direction = SOF_IPC_STREAM_PLAYBACK;

		if (mod->priv.cfg.ext_data->module_data_size >=
		    sizeof(struct snd_codec) + sizeof(uint32_t)) {
			memcpy(&direction, init_bytes + sizeof(struct snd_codec),
			       sizeof(direction));
		}

		if (direction == SOF_IPC_STREAM_CAPTURE) {
#if defined(CONFIG_FFMPEG_ENC)
			return ffmpeg_enc_mod_init(mod);
#else
			comp_err(dev, "capture (encode) requested but no encoders enabled");
			return -ENOTSUP;
#endif
		}
		if (direction != SOF_IPC_STREAM_PLAYBACK) {
			comp_err(dev, "invalid compress stream direction %u", direction);
			return -EINVAL;
		}
	}
#endif

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->backend = &ffmpeg_dec_backend;
	cd->codec = FFMPEG_DEC_DEFAULT_CODEC;
	cd->codec_ops = ffmpeg_dec_find_codec_ops(cd->codec);

#if CONFIG_IPC_MAJOR_4
	if (mod->priv.cfg.ext_data &&
	    mod->priv.cfg.ext_data->module_data &&
	    mod->priv.cfg.ext_data->module_data_size >= sizeof(struct snd_codec)) {
		const struct snd_codec *codec_params = mod->priv.cfg.ext_data->module_data;
		const struct ffmpeg_dec_codec_ops *ops;

		ops = ffmpeg_dec_find_codec_ops_by_snd_id(codec_params->id);
		if (!ops) {
			comp_err(dev, "unsupported or disabled snd_codec id 0x%x",
				 codec_params->id);
			mod_free(mod, cd);
			return -ENOTSUP;
		}
		cd->codec_ops = ops;
		cd->codec = ops->id;
		comp_info(dev, "codec selected from IPC: %s (%d, snd_codec id 0x%x)",
			  cd->codec_ops->name, cd->codec, codec_params->id);
	}
#endif

#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
	if (!cd->codec_ops) {
		comp_err(dev, "no decoder enabled for playback");
		mod_free(mod, cd);
		return -ENOTSUP;
	}
#endif

	/*
	 * Advertise the decoder's output block size so the DP scheduler sizes the
	 * ring buffer between this (data-processing) module and the downstream LL
	 * chain to hold several decoded frames (bind reads mpd.out_buff_size ->
	 * ring = 3 x this, see ipc4 helper.c). A whole decoded frame (up to ~24 ms
	 * of audio for MP3, or 120 ms for Opus) must fit in one drain, otherwise
	 * process() can only dribble a fraction per LL period and stalls waiting
	 * for the sink to drain at real time - serialising decode behind playback
	 * and running the stream slow. Size for ~2 frames (worst case, stereo S32),
	 * capped so the 3x ring stays bounded; clamp up to at least one frame for
	 * large blocks.
	 */
	frame_bytes = ffmpeg_dec_max_frame_samples(cd) * 2 /* ch */ * 4 /* S32 */;
	md->mpd.out_buff_size = 2 * frame_bytes;
	if (md->mpd.out_buff_size > 65536)		/* cap the 3x ring */
		md->mpd.out_buff_size = 65536;
	if (md->mpd.out_buff_size < frame_bytes)	/* but always hold >=1 frame */
		md->mpd.out_buff_size = frame_bytes;
	comp_info(dev, "DP out ring block=%u (frame=%u)", md->mpd.out_buff_size, frame_bytes);

	comp_info(dev, "backend '%s'", cd->backend->name);

	if (cd->backend->init) {
		ret = cd->backend->init(mod);
		if (ret) {
			comp_err(dev, "backend init failed %d", ret);
			mod_free(mod, cd);
			return ret;
		}
	}

#if CONFIG_IPC_MAJOR_4
	/* Pre-build the compress end-of-stream notification (used at drain). */
	cd->eos_msg = ffmpeg_dec_eos_notification_init(mod);
	if (!cd->eos_msg) {
		comp_err(dev, "failed to allocate EOS notification");
		if (cd->backend->free)
			cd->backend->free(mod);
		mod_free(mod, cd);
		return -ENOMEM;
	}
#endif

	return 0;
}

static inline size_t ffmpeg_dec_max(size_t a, size_t b)
{
	return a > b ? a : b;
}

/**
 * ffmpeg_dec_prepare() - Prepare the component for processing.
 * @mod: Pointer to module data.
 * @sources: Unused (input is a raw compressed byte stream).
 * @sinks: Output PCM sink array; sinks[0] provides the target PCM format.
 *
 * Caches the decoded PCM output format and opens the backend decoder.
 *
 * Return: Zero if success, otherwise error code.
 */
static int ffmpeg_dec_prepare(struct processing_module *mod,
			      struct sof_source **sources, int num_of_sources,
			      struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_sink *sink;

	comp_dbg(dev, "entry");

#if defined(CONFIG_FFMPEG_ENC)
	if (cd && cd->is_encoder)
		return ffmpeg_enc_mod_prepare(mod, sources, num_of_sources,
					      sinks, num_of_sinks);
#endif

	/*
	 * The PCM format the decoder must produce is that of the sink.
	 * Use the sink API directly to remain compliant with Pipeline 2.0.
	 */
	if (num_of_sinks < 1 || !sinks[0]) {
		comp_err(dev, "no sink connected");
		return -EINVAL;
	}
	sink = sinks[0];

	/* Cache the PCM format the decoder must produce for the pipeline. */
	cd->out_rate = sink_get_rate(sink);
	cd->out_channels = sink_get_channels(sink);
	cd->out_frame_fmt = sink_get_frm_fmt(sink);
	cd->out_frame_bytes = sink_get_frame_bytes(sink);

	comp_info(dev, "out rate %u ch %u fmt %d frame_bytes %u",
		  cd->out_rate, cd->out_channels, cd->out_frame_fmt,
		  cd->out_frame_bytes);

	/*
	 * NB: the backend decoder is NOT opened here. prepare() runs synchronously
	 * on the core's IPC/IDC worker thread (with a limited stack) during
	 * SET_PIPELINE_STATE, and before any stream data has arrived. Opening the
	 * codec (avcodec_open2) needs stream-embedded setup headers (e.g. FLAC
	 * STREAMINFO extracted by skip_container()) as well as the module's deep
	 * DP thread stack and table-initialization budget. Defer the open to
	 * ffmpeg_dec_process() on the module's own DP thread.
	 */

	/*
	 * Linear scratch buffers used by process(): a padded input bounce buffer
	 * (compressed source -> contiguous parser input) and a PCM staging buffer
	 * (one decoded frame, drained into the sink over several cycles). Keep any
	 * allocation from a previous prepare() to survive reset()/re-prepare().
	 */
	if (!cd->in_buf) {
		cd->in_buf = mod_alloc(mod, FFMPEG_DEC_IN_BLOCK_SIZE +
				       FFMPEG_DEC_INPUT_PADDING);
		if (!cd->in_buf)
			return -ENOMEM;
		cd->in_buf_size = FFMPEG_DEC_IN_BLOCK_SIZE;
	}
	{
		size_t max_samples = ffmpeg_dec_max_frame_samples(cd);
		size_t frame_bytes = cd->out_frame_bytes ? cd->out_frame_bytes :
				     (size_t)(cd->out_channels ? cd->out_channels : 2) * 4;
		size_t max_frame_bytes = max_samples * frame_bytes;
		size_t pcm_buf_needed = ffmpeg_dec_max(FFMPEG_DEC_PCM_BUF_SIZE, max_frame_bytes);

		if (!cd->pcm_buf || cd->pcm_buf_size < pcm_buf_needed) {
			if (cd->pcm_buf)
				mod_free(mod, cd->pcm_buf);
			cd->pcm_buf = mod_alloc(mod, pcm_buf_needed);
			if (!cd->pcm_buf)
				return -ENOMEM;
			cd->pcm_buf_size = pcm_buf_needed;
		}
	}
	cd->pcm_avail = 0;
	cd->pcm_rd = 0;
	cd->hdr_done = false;
	cd->skip_bytes = 0;
	cd->flac_parsing_meta = false;
	cd->flac_last_block = false;
	cd->draining = false;
	cd->drained = false;

	return 0;
}

/* Drain as much staged PCM as the sink can currently accept. */
static void ffmpeg_dec_drain_pcm(struct ffmpeg_dec_comp_data *cd,
				 struct sof_sink *sink)
{
	void *dst, *buf_start;
	size_t buf_size, n;
	int ret;

	n = ffmpeg_dec_min(cd->pcm_avail, sink_get_free_size(sink));
	if (!n)
		return;

	ret = sink_get_buffer(sink, n, &dst, &buf_start, &buf_size);
	if (ret)
		return;

	ffmpeg_dec_copy_to_circular(dst, buf_start, buf_size,
				    cd->pcm_buf + cd->pcm_rd, n);
	sink_commit_buffer(sink, n);
	cd->pcm_rd += n;
	cd->pcm_avail -= n;
}

/**
 * ffmpeg_dec_is_ready_to_process() - Gate the DP process() call.
 *
 * Ready when there is staged PCM still to drain, or fresh compressed input to
 * decode - in either case the sink must have room to receive something.
 */
static bool ffmpeg_dec_is_ready_to_process(struct processing_module *mod,
					   struct sof_source **sources,
					   int num_of_sources,
					   struct sof_sink **sinks,
					   int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);

#if defined(CONFIG_FFMPEG_ENC)
	if (cd && cd->is_encoder)
		return ffmpeg_enc_is_ready_to_process(mod, sources, num_of_sources,
						      sinks, num_of_sinks);
#endif

	if (num_of_sources < 1 || num_of_sinks < 1)
		return false;

	if (sink_get_free_size(sinks[0]) == 0)
		return false;

	if (cd->pcm_avail)
		return true;

	return source_get_data_available(sources[0]) > 0;
}

/**
 * ffmpeg_dec_process() - Decode compressed input into PCM (DP sink/source).
 * @mod: Pointer to module data.
 * @sources: sources[0] is the compressed elementary-stream input.
 * @sinks: sinks[0] receives interleaved PCM.
 *
 * As a DP (data-processing domain) module the decoder uses the sink/source
 * circular-buffer API. Each cycle either drains previously decoded PCM into the
 * sink, or pulls a block of compressed bytes into a linear bounce buffer, has
 * the backend decode one frame into the PCM staging buffer, and drains what
 * fits. Decoded PCM larger than the sink's free space is carried to the next
 * cycle rather than dropped.
 *
 * Return: Zero if success, otherwise error code.
 */
static int ffmpeg_dec_process(struct processing_module *mod,
			      struct sof_source **sources, int num_of_sources,
			      struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_source *src;
	struct sof_sink *sink;
	const void *sp, *sstart;
	size_t sbytes, avail, req;
	size_t consumed = 0, produced = 0;
	int ret;

#if defined(CONFIG_FFMPEG_ENC)
	if (cd && cd->is_encoder)
		return ffmpeg_enc_mod_process(mod, sources, num_of_sources,
					      sinks, num_of_sinks);
#endif

	if (num_of_sources < 1 || num_of_sinks < 1)
		return -EINVAL;

	src = sources[0];
	sink = sinks[0];
	enum sof_audio_buffer_state src_state = source_get_state(src);
	bool host_eos = src_state == AUDIOBUF_STATE_END_OF_STREAM ||
			src_state == AUDIOBUF_STATE_END_OF_STREAM_FLUSH;

	/* Drain leftover PCM from a previous decode before consuming more input. */
	if (cd->pcm_avail) {
		ffmpeg_dec_drain_pcm(cd, sink);
		return 0;
	}

	/* One-time: strip the container header so the parser sees frames. */
	if (!cd->hdr_done) {
		if (cd->codec_ops && cd->codec_ops->skip_container) {
			ret = cd->codec_ops->skip_container(mod, src);
			if (ret < 0)
				return ret;	/* -ENODATA: wait for the rest of the header */
			cd->hdr_done = true;
			return 0;		/* decode frames on subsequent cycles */
		}
		cd->hdr_done = true;
	}

	/*
	 * Lazy backend open, one-time, on the DP thread. Run after container
	 * stripping so stream-embedded setup headers (e.g. FLAC STREAMINFO) are
	 * extracted before avcodec_open2(), and on the module's own DP thread
	 * (with its dedicated decoder stack) rather than blocking the synchronous
	 * SET_PIPELINE_STATE IPC/IDC worker during prepare(). reset() flushes but
	 * keeps the decoder open (configured stays true), so this runs only on the
	 * first process() after a fresh prepare().
	 */
	if (!cd->configured) {
		if (cd->backend->configure) {
			ret = cd->backend->configure(mod);
			if (ret) {
				comp_err(dev, "backend configure failed %d", ret);
				return ret;
			}
		}
		cd->configured = true;
		return 0;	/* decode on the next cycle */
	}

	avail = source_get_data_available(src);
	if (!avail) {
#if CONFIG_IPC_MAJOR_4
		/*
		 * Under a compress drain the host stops delivering data once the
		 * file has been fully consumed. Flush parser and codec to drain
		 * all remaining frames before signaling EOS.
		 */
		if (host_eos && !cd->pcm_avail) {
			if (!cd->drained) {
				size_t c_consumed = 0;
				size_t c_produced = 0;

				ret = cd->backend->decode(mod, NULL, 0, &c_consumed,
							  cd->pcm_buf, cd->pcm_buf_size,
							  &c_produced);
				if (ret == 0 && c_produced > 0) {
					cd->pcm_rd = 0;
					cd->pcm_avail = c_produced;
					ffmpeg_dec_drain_pcm(cd, sink);
					return 0;
				}
				cd->drained = true;
			}
			ffmpeg_dec_signal_eos(mod, sink);
		}
#endif
		return -ENODATA;
	}

	/* Pull one contiguous, padded block of compressed input. */
	req = ffmpeg_dec_min(avail, cd->in_buf_size);
	ret = source_get_data(src, req, &sp, &sstart, &sbytes);
	if (ret)
		return ret;

	ffmpeg_dec_copy_from_circular(cd->in_buf, sp, sstart, sbytes, req);
	memset(cd->in_buf + req, 0, FFMPEG_DEC_INPUT_PADDING);

	/*
	 * Fast-path direct-to-sink decode:
	 * When no staged PCM remains from a previous cycle and the sink circular
	 * buffer has sufficient contiguous space to hold a full decoded frame,
	 * decode and interleave directly into the sink buffer. This bypasses
	 * cd->pcm_buf and eliminates an intermediate copy per audio sample.
	 */
	bool direct_sink = false;
	uint8_t *dec_out = cd->pcm_buf;
	size_t dec_out_size = cd->pcm_buf_size;
	size_t max_frame = (size_t)ffmpeg_dec_max_frame_samples(cd) *
			   (cd->out_frame_bytes ? (size_t)cd->out_frame_bytes : 8);

	if (!cd->pcm_avail && sink_get_free_size(sink) >= max_frame) {
		void *dst, *buf_start;
		size_t buf_size;

		if (sink_get_buffer(sink, max_frame, &dst, &buf_start, &buf_size) == 0) {
			size_t contiguous = (size_t)((const uint8_t *)buf_start + buf_size -
						     (const uint8_t *)dst);

			if (contiguous >= max_frame) {
				direct_sink = true;
				dec_out = (uint8_t *)dst;
				dec_out_size = contiguous;
			} else {
				/* Not enough contiguous space before circular wrap */
				sink_commit_buffer(sink, 0);
			}
		}
	}

	ret = cd->backend->decode(mod, cd->in_buf, req, &consumed,
				  dec_out, dec_out_size, &produced);
	source_release_data(src, consumed);
	if (ret) {
		if (direct_sink)
			sink_commit_buffer(sink, 0);
		comp_err(dev, "decode failed %d", ret);
		return ret;
	}

	if (direct_sink) {
		/* Decoded PCM is already in the sink buffer - commit directly! */
		sink_commit_buffer(sink, produced);
		cd->pcm_rd = 0;
		cd->pcm_avail = 0;
	} else {
		/* Fallback: PCM was staged in cd->pcm_buf; drain to sink */
		cd->pcm_rd = 0;
		cd->pcm_avail = produced;
		if (cd->pcm_avail)
			ffmpeg_dec_drain_pcm(cd, sink);
	}
	/*
	 * produced == 0 is NOT end-of-stream. A compressed chunk can end mid-frame
	 * (variable-size ADTS AAC frames routinely do), leaving the parser holding a
	 * partial frame it completes from the next chunk: input is consumed while no
	 * PCM comes out. The real drain completes on a later cycle once the input is
	 * genuinely exhausted (avail == 0 with expect_eos, handled at the top of
	 * process()). Signalling EOS on a transient produced == 0 truncated AAC to
	 * its first few frames, since the host sets expect_eos as soon as it has
	 * written the whole (buffer-resident) file, long before decode catches up.
	 */

#if CONFIG_IPC_MAJOR_4
	/*
	 * End-of-stream completion. If the host has flagged EOS and a cycle
	 * produced no PCM and no staged PCM remains, enter draining mode to
	 * flush the parser and codec before signaling EOS.
	 */
	if (host_eos && !produced && !cd->pcm_avail) {
		if (!cd->drained) {
			size_t c_consumed = 0;
			size_t c_flushed = 0;

			ret = cd->backend->decode(mod, NULL, 0, &c_consumed,
						  cd->pcm_buf, cd->pcm_buf_size,
						  &c_flushed);
			if (ret == 0 && c_flushed > 0) {
				cd->pcm_rd = 0;
				cd->pcm_avail = c_flushed;
				ffmpeg_dec_drain_pcm(cd, sink);
				return 0;
			}
			cd->drained = true;
		}
		ffmpeg_dec_signal_eos(mod, sink);
	}
#endif

	return 0;
}

/**
 * ffmpeg_dec_set_config() - Receive the codec setup header (extradata).
 *
 * Reassembles a possibly fragmented binary configuration via the common
 * module_set_configuration() helper, then stores the whole reassembled blob as
 * codec extradata (e.g. FLAC STREAMINFO). Mirrors the DTS codec config path.
 */
__cold static int
ffmpeg_dec_set_config(struct processing_module *mod, uint32_t config_id,
		      enum module_cfg_fragment_position pos, uint32_t data_offset_size,
		      const uint8_t *fragment, size_t fragment_size, uint8_t *response,
		      size_t response_size)
{
	struct comp_dev *dev = mod->dev;
	struct module_config *config = &mod->priv.cfg;
	int ret;

	assert_can_be_cold();

	ret = module_set_configuration(mod, config_id, pos, data_offset_size, fragment,
				       fragment_size, response, response_size);
	if (ret < 0) {
		comp_err(dev, "module_set_configuration failed %d", ret);
		return ret;
	}

	/* Wait until the whole (possibly fragmented) blob has been received. */
	if (pos != MODULE_CFG_FRAGMENT_LAST && pos != MODULE_CFG_FRAGMENT_SINGLE)
		return 0;

	/*
	 * module_load_config() sets config->size to the payload size and
	 * config->data to the payload itself (the kernel has already stripped
	 * the sof_abi_hdr) - there is no inline header to skip. The payload is
	 * the raw codec setup blob (e.g. the 34-byte FLAC STREAMINFO).
	 */
	if (!config->size || !config->data) {
		comp_warn(dev, "empty codec config");
		return 0;
	}

	return ffmpeg_dec_store_extradata(mod, config->data, config->size);
}

/**
 * ffmpeg_dec_reset() - Reset the component to a re-preparable state.
 * @mod: Pointer to module data.
 *
 * Flushes decoder state but keeps allocations so the pipeline can restart.
 *
 * Return: Zero if success, otherwise error code.
 */
static int ffmpeg_dec_reset(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "entry");

#if defined(CONFIG_FFMPEG_ENC)
	if (cd && cd->is_encoder)
		return ffmpeg_enc_mod_reset(mod);
#endif

	/* Discard any PCM staged mid-stream; the pipeline is restarting. */
	cd->pcm_avail = 0;
	cd->pcm_rd = 0;
	cd->hdr_done = false;
	cd->skip_bytes = 0;
	cd->flac_parsing_meta = false;
	cd->flac_last_block = false;
	cd->draining = false;
	cd->drained = false;
#if CONFIG_IPC_MAJOR_4
	cd->eos_sent = false;
#endif

	if (cd->backend->reset)
		return cd->backend->reset(mod);

	return 0;
}

/**
 * ffmpeg_dec_free() - Free dynamic allocations.
 * @mod: Pointer to module data.
 *
 * Return: Zero if success, otherwise error code.
 */
__cold static int ffmpeg_dec_free(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();
	comp_dbg(mod->dev, "entry");

#if defined(CONFIG_FFMPEG_ENC)
	if (cd && cd->is_encoder)
		return ffmpeg_enc_mod_free(mod);
#endif

	if (cd->backend->free)
		cd->backend->free(mod);

#if CONFIG_IPC_MAJOR_4
	if (cd->eos_msg)
		mod_ipc_msg_free(mod, cd->eos_msg);
#endif

	if (cd->pcm_buf)
		mod_free(mod, cd->pcm_buf);
	if (cd->in_buf)
		mod_free(mod, cd->in_buf);

	mod_free(mod, cd);
	return 0;
}

/* This defines the module operations */
static const struct module_interface ffmpeg_dec_interface = {
	.init = ffmpeg_dec_init,
	.prepare = ffmpeg_dec_prepare,
	.process = ffmpeg_dec_process,
	.is_ready_to_process = ffmpeg_dec_is_ready_to_process,
	.set_configuration = ffmpeg_dec_set_config,
	.reset = ffmpeg_dec_reset,
	.free = ffmpeg_dec_free
};

/* If COMP_FFMPEG_DEC is =m in Kconfig this is built as a loadable LLEXT module. */
#if CONFIG_COMP_FFMPEG_DEC_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("FFMPGDEC", &ffmpeg_dec_interface, 1,
				  SOF_REG_UUID(ffmpeg_dec), 40);

SOF_LLEXT_BUILDINFO;

#else

/* Only used for the module adapter trace context, soon to be deprecated */
DECLARE_TR_CTX(ffmpeg_dec_tr, SOF_UUID(ffmpeg_dec_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(ffmpeg_dec_interface, ffmpeg_dec_uuid, ffmpeg_dec_tr);
SOF_MODULE_INIT(ffmpeg_dec, sys_comp_module_ffmpeg_dec_interface_init);

#endif
