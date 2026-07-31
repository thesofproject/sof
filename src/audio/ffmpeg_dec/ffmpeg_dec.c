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
#include <sof/audio/source_api.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <errno.h>
#include "ffmpeg_dec.h"

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

/* Decoder-mode ops (compressed -> PCM). In filter mode the module uses the
 * avfilter-graph PCM effect ops (ffmpeg_dec-filter.c); in encode mode the
 * PCM->compressed ops (ffmpeg_dec-encode.c) instead.
 */
#if !CONFIG_FFMPEG_DEC_FILTER_MODE && !CONFIG_FFMPEG_DEC_ENCODE_MODE
int ffmpeg_dec_store_extradata(struct processing_module *mod,
			       const uint8_t *data, size_t size)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint8_t *buf;

	if (!size)
		return 0;

	if (size > FFMPEG_DEC_MAX_EXTRADATA) {
		comp_err(dev, "extradata too large: %zu", size);
		return -EINVAL;
	}

	/* Replace any previously stored setup header. */
	if (cd->extradata) {
		mod_free(mod, cd->extradata);
		cd->extradata = NULL;
		cd->extradata_size = 0;
	}

	buf = mod_alloc(mod, size);
	if (!buf)
		return -ENOMEM;

	memcpy_s(buf, size, data, size);
	cd->extradata = buf;
	cd->extradata_size = size;
	comp_info(dev, "stored %zu bytes of codec extradata", size);
	return 0;
}

#if CONFIG_IPC_MAJOR_4
/*
 * Build the module notification the driver expects to unblock compress_drain().
 * The kernel's compress EOS handler (sof_ipc4_compr_drain_done) matches on the
 * COMPR "magic" event id, so use the same template the Cadence decoder does.
 */
__cold static struct ipc_msg *ffmpeg_dec_eos_notification_init(struct processing_module *mod)
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

	msg = ipc_msg_w_ext_init(primary.dat, 0, sizeof(*msg_data));
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
static void ffmpeg_dec_signal_eos(struct processing_module *mod, struct sof_sink *sink)
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

/*
 * Largest number of PCM samples-per-channel one decoded frame of @codec can
 * yield. Used only to size the DP output ring buffer (see ffmpeg_dec_init);
 * a generous value is harmless, an undersized one throttles playback.
 */
static uint32_t ffmpeg_dec_max_frame_samples(enum ffmpeg_dec_codec codec)
{
	switch (codec) {
	case FFMPEG_DEC_CODEC_FLAC:	return 4096;	/* typical max block */
	case FFMPEG_DEC_CODEC_AAC:	return 2048;	/* HE-AAC (SBR) */
	case FFMPEG_DEC_CODEC_OPUS:	return 2880;	/* 60 ms @ 48k */
	case FFMPEG_DEC_CODEC_MP3:	return 1152;	/* MPEG-1 layer 3 */
	default:			return 1152;
	}
}

/**
 * ffmpeg_dec_init() - Initialize the ffmpeg_dec component.
 * @mod: Pointer to module data.
 *
 * Allocates private data and hands off to the selected decode backend for its
 * one-time initialization. __cold marks this non-critical path for slower DRAM.
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

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->backend = &ffmpeg_dec_backend;
	/* TODO: derive codec id from topology/IPC init config. Until then use the
	 * default from the Kconfig decoder selection (first enabled).
	 */
	cd->codec = FFMPEG_DEC_DEFAULT_CODEC;

	/*
	 * Advertise the decoder's output block size so the DP scheduler sizes the
	 * ring buffer between this (data-processing) module and the downstream LL
	 * chain to hold several decoded frames (bind reads mpd.out_buff_size ->
	 * ring = 3 x this, see ipc4 helper.c). A whole decoded frame (up to ~24 ms
	 * of audio for MP3) must fit in one drain, otherwise process() can only
	 * dribble a fraction per LL period and stalls waiting for the sink to
	 * drain at real time - serialising decode behind playback and running the
	 * stream slow. Size for ~2 frames (worst case, stereo S32), capped so the
	 * 3x ring stays bounded; clamp up to at least one frame for large blocks.
	 */
	frame_bytes = ffmpeg_dec_max_frame_samples(cd->codec) * 2 /* ch */ * 4 /* S32 */;
	md->mpd.out_buff_size = 2 * frame_bytes;
	if (md->mpd.out_buff_size > 32768)		/* cap the 3x ring */
		md->mpd.out_buff_size = 32768;
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
	struct comp_buffer *sinkb;
	const struct audio_stream *stream;
	int ret;

	comp_dbg(dev, "entry");

	/*
	 * The PCM format the decoder must produce is that of the sink (data
	 * consumer) buffer. Read it from the component device rather than the
	 * sinks[] arg so the audio_stream getters (rate/channels/frame fmt) are
	 * all available in one place.
	 */
	sinkb = comp_dev_get_first_data_consumer(dev);
	if (!sinkb) {
		comp_err(dev, "no sink buffer connected");
		return -EINVAL;
	}
	stream = &sinkb->stream;

	/* Cache the PCM format the decoder must produce for the pipeline. */
	cd->out_rate = audio_stream_get_rate(stream);
	cd->out_channels = audio_stream_get_channels(stream);
	cd->out_frame_fmt = audio_stream_get_frm_fmt(stream);
	cd->out_frame_bytes = audio_stream_frame_bytes(stream);

	comp_info(dev, "out rate %u ch %u fmt %d frame_bytes %u",
		  cd->out_rate, cd->out_channels, cd->out_frame_fmt,
		  cd->out_frame_bytes);

	/*
	 * NB: the backend decoder is NOT opened here. prepare() is dispatched to
	 * this component's core over IDC and runs on that core's single IDC worker
	 * thread; the codec open (avcodec_open2) builds large trig tables and can
	 * take hundreds of ms (~580 ms measured for mp3). Doing that here would
	 * monopolise the IDC worker, so a cross-core prepare/trigger on the
	 * initiator core stalls or times out (-EAGAIN) and crashes on teardown.
	 * Defer the open to the first process() call, which runs on the module's
	 * own DP thread (deep 64 KiB stack, off the IDC critical path). See the
	 * lazy-open block in ffmpeg_dec_process().
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
	if (!cd->pcm_buf) {
		cd->pcm_buf = mod_alloc(mod, FFMPEG_DEC_PCM_BUF_SIZE);
		if (!cd->pcm_buf)
			return -ENOMEM;
		cd->pcm_buf_size = FFMPEG_DEC_PCM_BUF_SIZE;
	}
	cd->pcm_avail = 0;
	cd->pcm_rd = 0;
	cd->hdr_done = false;

	return 0;
}

static inline size_t ffmpeg_dec_min(size_t a, size_t b)
{
	return a < b ? a : b;
}

/* Copy @bytes out of a circular source buffer (starting at @src) into the
 * linear @dst, wrapping at the buffer end if necessary.
 */
static void ffmpeg_dec_copy_from_circular(void *dst, const void *src,
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
static void ffmpeg_dec_copy_to_circular(void *dst, const void *buf_start,
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

/*
 * Consume and discard a container header from the source so the decoder is fed a
 * bare frame elementary stream. Handles two prefixes:
 *   - "fLaC" : FLAC magic + metadata blocks (STREAMINFO reaches the decoder
 *     separately as extradata, so the whole header is skipped here).
 *   - "ID3"  : an ID3v2 tag prepended to an MP3 elementary stream. The mpegaudio
 *     parser syncs on the 0xFFEx frame header and would mis-parse the tag, so the
 *     syncsafe-encoded tag length is skipped.
 *
 * cplay/tinycompress hand the firmware the whole *file*, but libavcodec's raw
 * parsers sync on the frame marker and error ("Error buffering data") if fed the
 * container/tag prefix.
 *
 * The header is far smaller than in_buf_size and the entire clip is already
 * resident in the source ring, so a single pass suffices. Returns 0 once skipped
 * (or when there is no container marker), -ENODATA while the full header is not yet
 * buffered, or a negative errno on a malformed/oversized header.
 */
static int ffmpeg_dec_skip_container(struct ffmpeg_dec_comp_data *cd,
				     struct comp_dev *dev, struct sof_source *src)
{
	const void *sp, *sstart;
	size_t sbytes, avail, req, off;
	int ret;

	avail = source_get_data_available(src);
	if (avail < 4)
		return -ENODATA;

	req = ffmpeg_dec_min(avail, cd->in_buf_size);
	ret = source_get_data(src, req, &sp, &sstart, &sbytes);
	if (ret)
		return ret;
	ffmpeg_dec_copy_from_circular(cd->in_buf, sp, sstart, sbytes, req);

	/*
	 * ID3v2 tag (common on real-world MP3 files): 10-byte header ("ID3",
	 * 2-byte version, 1-byte flags, 4-byte syncsafe size), then the tag body,
	 * then (if flags bit 0x10) a 10-byte footer. Skip the whole tag so the
	 * mpegaudio parser sees frames.
	 */
	if (memcmp(cd->in_buf, "ID3", 3) == 0) {
		uint32_t tag;

		if (req < 10) {
			source_release_data(src, 0);
			return -ENODATA;
		}
		/* syncsafe: 7 bits per byte */
		tag = ((uint32_t)(cd->in_buf[6] & 0x7f) << 21) |
		      ((uint32_t)(cd->in_buf[7] & 0x7f) << 14) |
		      ((uint32_t)(cd->in_buf[8] & 0x7f) << 7) |
		      (uint32_t)(cd->in_buf[9] & 0x7f);
		off = 10 + tag + ((cd->in_buf[5] & 0x10) ? 10 : 0);
		if (off > req) {
			source_release_data(src, 0);
			comp_err(dev, "ID3v2 tag (%zu) exceeds input block (%zu)", off, req);
			return -EINVAL;
		}
		source_release_data(src, off);
		comp_info(dev, "skipped %zu-byte ID3v2 tag", off);
		return 0;
	}

	/* No container marker: the stream already starts with frames. */
	if (memcmp(cd->in_buf, "fLaC", 4) != 0) {
		source_release_data(src, 0);
		return 0;
	}

	/* Walk metadata blocks: 1 byte (last<<7 | type) + 3-byte big-endian len. */
	off = 4;
	for (;;) {
		uint8_t hdr;
		uint32_t len;

		if (off + 4 > req) {
			source_release_data(src, 0);
			comp_err(dev, "FLAC header exceeds %zu-byte input block", req);
			return -EINVAL;
		}
		hdr = cd->in_buf[off];
		len = ((uint32_t)cd->in_buf[off + 1] << 16) |
		      ((uint32_t)cd->in_buf[off + 2] << 8) |
		      (uint32_t)cd->in_buf[off + 3];
		off += 4 + len;
		if (hdr & 0x80)		/* last-metadata-block flag */
			break;
	}

	if (off > req) {
		source_release_data(src, 0);
		comp_err(dev, "FLAC header (%zu) exceeds input block (%zu)", off, req);
		return -EINVAL;
	}

	source_release_data(src, off);
	comp_info(dev, "skipped %zu-byte FLAC container header", off);
	return 0;
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

	if (num_of_sources < 1 || num_of_sinks < 1)
		return -EINVAL;

	src = sources[0];
	sink = sinks[0];
	enum sof_audio_buffer_state src_state = source_get_state(src);
	bool host_eos = src_state == AUDIOBUF_STATE_END_OF_STREAM ||
			src_state == AUDIOBUF_STATE_END_OF_STREAM_FLUSH;

	/*
	 * Lazy backend open, one-time, on the DP thread. prepare() deliberately
	 * skips avcodec_open2 because it runs on the core's single IDC worker where
	 * a ~580 ms mp3 table build would stall/timeout a cross-core prepare or
	 * trigger. Here we are on the module's own DP thread with a deep stack, so
	 * the open is off the IDC critical path. reset() flushes but keeps the
	 * decoder open (configured stays true), so this runs only on the first
	 * process() after a fresh prepare().
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

	/* Drain leftover PCM from a previous decode before consuming more input. */
	if (cd->pcm_avail) {
		ffmpeg_dec_drain_pcm(cd, sink);
		return 0;
	}

	/* One-time: strip the FLAC container header so the parser sees frames. */
	if (!cd->hdr_done) {
		ret = ffmpeg_dec_skip_container(cd, dev, src);
		if (ret < 0)
			return ret;	/* -ENODATA: wait for the rest of the header */
		cd->hdr_done = true;
		return 0;		/* decode frames on subsequent cycles */
	}

	avail = source_get_data_available(src);
	if (!avail) {
#if CONFIG_IPC_MAJOR_4
		/*
		 * Under a compress drain the host stops delivering data once the
		 * file has been fully consumed. With no input left and no staged
		 * PCM, the stream is flushed - complete the drain.
		 */
		if (host_eos)
			ffmpeg_dec_signal_eos(mod, sink);
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

	ret = cd->backend->decode(mod, cd->in_buf, req, &consumed,
				  cd->pcm_buf, cd->pcm_buf_size, &produced);
	source_release_data(src, consumed);
	if (ret) {
		comp_err(dev, "decode failed %d", ret);
		return ret;
	}

	cd->pcm_rd = 0;
	cd->pcm_avail = produced;

	/* Drain whatever fits this cycle; the rest goes out on the next one. */
	if (cd->pcm_avail)
		ffmpeg_dec_drain_pcm(cd, sink);
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
	 * End-of-stream completion. The host copier flips the source to
	 * END_OF_STREAM only after delivering the whole committed file, so every
	 * real frame is already buffered and decodes (produced > 0) before this
	 * point. The free-running HDA gateway then keeps re-presenting stale 384 B
	 * junk chunks that avail never drains and that yield no PCM - so once we
	 * are at EOS and a cycle produces nothing, the real stream is exhausted:
	 * signal EOS to complete the host compress_drain(). ffmpeg_dec_signal_eos
	 * is idempotent (guarded by cd->eos_sent).
	 */
	if (host_eos && !produced)
		ffmpeg_dec_signal_eos(mod, sink);
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

	/* Discard any PCM staged mid-stream; the pipeline is restarting. */
	cd->pcm_avail = 0;
	cd->pcm_rd = 0;
	cd->hdr_done = false;
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

	if (cd->backend->free)
		cd->backend->free(mod);

#if CONFIG_IPC_MAJOR_4
	if (cd->eos_msg)
		ipc_msg_free(cd->eos_msg);
#endif

	if (cd->pcm_buf)
		mod_free(mod, cd->pcm_buf);
	if (cd->in_buf)
		mod_free(mod, cd->in_buf);
	if (cd->extradata)
		mod_free(mod, cd->extradata);

	mod_free(mod, cd);
	return 0;
}
#endif /* !CONFIG_FFMPEG_DEC_FILTER_MODE && !CONFIG_FFMPEG_DEC_ENCODE_MODE */

/* This defines the module operations */
#if CONFIG_FFMPEG_DEC_ENCODE_MODE
/* PCM -> compressed encoder (ffmpeg_dec-encode.c). */
static const struct module_interface ffmpeg_dec_interface = {
	.init = ffmpeg_enc_mod_init,
	.prepare = ffmpeg_enc_mod_prepare,
	.process_raw_data = ffmpeg_enc_mod_process,
	.free = ffmpeg_enc_mod_free
};
#elif CONFIG_FFMPEG_DEC_FILTER_MODE
/* PCM source/sink effect driving an avfilter graph (ffmpeg_dec-filter.c). */
static const struct module_interface ffmpeg_dec_interface = {
	.init = ffmpeg_af_mod_init,
	.prepare = ffmpeg_af_mod_prepare,
	.process = ffmpeg_af_mod_process,
	.free = ffmpeg_af_mod_free
};
#else
static const struct module_interface ffmpeg_dec_interface = {
	.init = ffmpeg_dec_init,
	.prepare = ffmpeg_dec_prepare,
	.process = ffmpeg_dec_process,
	.is_ready_to_process = ffmpeg_dec_is_ready_to_process,
	.set_configuration = ffmpeg_dec_set_config,
	.reset = ffmpeg_dec_reset,
	.free = ffmpeg_dec_free
};
#endif

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
