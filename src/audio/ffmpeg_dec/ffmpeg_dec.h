/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * FFmpeg (libavcodec) audio decoder wrapper for SOF.
 *
 * This module adapts SOF's module_interface to libavcodec's send-packet /
 * receive-frame decode API. Compressed elementary-stream bytes arrive on the
 * input (raw data) buffer, are parsed into codec frames, decoded to interleaved
 * PCM and written to the output buffer.
 *
 * The actual decode work is delegated to a pluggable "backend" so the SOF glue
 * can be validated with a dependency-free stub (ffmpeg_dec-stub.c) before the
 * real libavcodec backend (ffmpeg_dec-ffmpeg.c) is linked in.
 */
#ifndef __SOF_AUDIO_FFMPEG_DEC_H__
#define __SOF_AUDIO_FFMPEG_DEC_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Maximum codec setup header (extradata) we accept, e.g. FLAC STREAMINFO. */
#define FFMPEG_DEC_MAX_EXTRADATA	4096

/*
 * Compressed input is pulled from the circular source buffer into a linear,
 * padded bounce buffer one block at a time (the parser needs contiguous bytes
 * and libavcodec over-reads past the packet by AV_INPUT_BUFFER_PADDING_SIZE).
 */
#define FFMPEG_DEC_IN_BLOCK_SIZE	4096
#define FFMPEG_DEC_INPUT_PADDING	64

/*
 * A single decoded frame (e.g. a 4096-sample FLAC block, stereo S32 = 32 KiB)
 * can exceed the sink's free space, so decode into a linear staging buffer and
 * drain it into the circular sink across as many process() cycles as needed.
 */
#define FFMPEG_DEC_PCM_BUF_SIZE		65536

/* Which codec this instance decodes. Kept as an explicit enum so it can be
 * carried in topology/IPC config and mapped to an AVCodecID by the backend.
 */
enum ffmpeg_dec_codec {
	FFMPEG_DEC_CODEC_NONE = 0,
	FFMPEG_DEC_CODEC_FLAC,
	FFMPEG_DEC_CODEC_AAC,
	FFMPEG_DEC_CODEC_OPUS,
	FFMPEG_DEC_CODEC_MP3,
};

/* Default codec for a new instance, picked from the Kconfig selection until the
 * codec is carried per-instance from topology/IPC config. First enabled wins.
 */
#if defined(CONFIG_FFMPEG_DEC_FLAC) || defined(CONFIG_COMP_FFMPEG_DEC_STUB)
#define FFMPEG_DEC_DEFAULT_CODEC	FFMPEG_DEC_CODEC_FLAC
#elif defined(CONFIG_FFMPEG_DEC_AAC)
#define FFMPEG_DEC_DEFAULT_CODEC	FFMPEG_DEC_CODEC_AAC
#elif defined(CONFIG_FFMPEG_DEC_OPUS)
#define FFMPEG_DEC_DEFAULT_CODEC	FFMPEG_DEC_CODEC_OPUS
#elif defined(CONFIG_FFMPEG_DEC_MP3)
#define FFMPEG_DEC_DEFAULT_CODEC	FFMPEG_DEC_CODEC_MP3
#else
#define FFMPEG_DEC_DEFAULT_CODEC	FFMPEG_DEC_CODEC_NONE
#endif

struct ffmpeg_dec_comp_data;
struct ffmpeg_enc_data;
struct ipc_msg;
struct snd_codec;
struct sof_source;
struct sof_sink;

/* Module-interface entry points for encode mode (CONFIG_FFMPEG_ENC):
 * PCM in -> compressed elementary stream out (ffmpeg-encode.c),
 * selected at runtime when the compress stream direction is capture.
 */
int ffmpeg_enc_mod_init(struct processing_module *mod);
int ffmpeg_enc_mod_prepare(struct processing_module *mod,
			   struct sof_source **sources, int num_of_sources,
			   struct sof_sink **sinks, int num_of_sinks);
int ffmpeg_enc_mod_process(struct processing_module *mod,
			   struct sof_source **sources, int num_of_sources,
			   struct sof_sink **sinks, int num_of_sinks);
bool ffmpeg_enc_is_ready_to_process(struct processing_module *mod,
				    struct sof_source **sources, int num_of_sources,
				    struct sof_sink **sinks, int num_of_sinks);
int ffmpeg_enc_mod_reset(struct processing_module *mod);
int ffmpeg_enc_mod_free(struct processing_module *mod);

/**
 * struct ffmpeg_enc_codec_ops - Per-codec encoder operations.
 * @id:              SOF internal codec enum (enum ffmpeg_dec_codec).
 * @snd_codec_id:    ALSA compress SND_AUDIOCODEC_* identifier.
 * @name:            Human-readable codec name (e.g. "mp3", "aac").
 * @validate_params: Optional hook to validate IPC4 struct snd_codec parameters.
 * @init:            Allocate and initialize codec-private encoder state in e->codec_priv.
 * @configure:       Open/configure encoder for cd->out_rate, cd->out_channels, e->bit_rate
 *                   and set e->nb (samples per channel per frame).
 * @encode_frame:    Encode one full frame of e->nb samples from e->pcm_in into e->out_stage.
 * @reset:           Optional hook to reset encoder state across stream resets.
 * @free:            Tear down and free e->codec_priv.
 */
struct ffmpeg_enc_codec_ops {
	enum ffmpeg_dec_codec id;
	uint32_t snd_codec_id;
	const char *name;
	int (*validate_params)(struct processing_module *mod,
			       const struct snd_codec *codec_params);
	int (*init)(struct processing_module *mod, struct ffmpeg_enc_data *e);
	int (*configure)(struct processing_module *mod, struct ffmpeg_enc_data *e);
	int (*encode_frame)(struct processing_module *mod, struct ffmpeg_enc_data *e);
	void (*reset)(struct processing_module *mod, struct ffmpeg_enc_data *e);
	void (*free)(struct processing_module *mod, struct ffmpeg_enc_data *e);
};

/**
 * struct ffmpeg_enc_data - Common encoder state across all encode codecs.
 * @codec_ops:    Selected per-codec encoder operations.
 * @codec_priv:   Codec-private encoder state allocated by @codec_ops->init().
 * @bit_rate:     Target encoder bit rate in bps.
 * @req_rate:     Requested sample rate from IPC struct snd_codec (0 if unspecified).
 * @req_channels: Requested channel count from IPC struct snd_codec (0 if unspecified).
 * @req_ch_mode:  Requested channel mode from IPC struct snd_codec (0 if unspecified).
 * @src_fmt:      SOF input sample format (enum sof_ipc_frame).
 * @frame_bytes:  SOF input frame size in bytes (all channels).
 * @nb:           Encoder frame size in samples per channel.
 * @need:         Source PCM bytes for one encoder frame (nb * frame_bytes).
 * @pcm_in:       Linear interleaved input PCM buffer for one encoder frame.
 * @pcm_fill:     Bytes accumulated in @pcm_in so far (< @need).
 * @out_stage:    Linear compressed output staging buffer.
 * @out_cap:      Capacity of @out_stage in bytes.
 * @out_avail:    Staged compressed bytes not yet drained to sink.
 * @out_rd:       Read offset of next byte to drain from @out_stage.
 */
struct ffmpeg_enc_data {
	const struct ffmpeg_enc_codec_ops *codec_ops;
	void *codec_priv;
	uint32_t bit_rate;
	uint32_t req_rate;
	uint32_t req_channels;
	uint32_t req_ch_mode;
	int src_fmt;
	int frame_bytes;
	int nb;
	size_t need;
	uint8_t *pcm_in;
	size_t pcm_fill;
	uint8_t *out_stage;
	size_t out_cap;
	size_t out_avail;
	size_t out_rd;
};

extern const struct ffmpeg_enc_codec_ops ffmpeg_enc_mp3_ops;
extern const struct ffmpeg_enc_codec_ops ffmpeg_enc_aac_ops;

void ffmpeg_enc_pcm_to_s16_interleaved(const uint8_t *pcm_in, int16_t *dst,
				       size_t total_samples, int src_fmt);
void ffmpeg_enc_pcm_to_s16_planar(const uint8_t *pcm_in, uint8_t * const *planes,
				  int nb_samples, int ch, int src_fmt);
void ffmpeg_enc_stage_packet(struct processing_module *mod, struct ffmpeg_enc_data *e,
			     const uint8_t *data, size_t len);

/**
 * struct ffmpeg_dec_codec_ops - Per-codec stream and configuration operations.
 * @id:                SOF internal codec enum (enum ffmpeg_dec_codec).
 * @snd_codec_id:      ALSA compress SND_AUDIOCODEC_* identifier.
 * @name:              Human-readable codec name (e.g. "flac", "mp3", "aac", "opus").
 * @decoder_name:      Optional explicit libavcodec decoder name (e.g. "aac_fixed"), or NULL.
 * @max_frame_samples: Return maximum PCM samples per channel per decoded frame.
 * @skip_container:    Optional one-time stream container/header parser called before
 *                     decoder open; returns 0 when ready to decode frames, -ENODATA
 *                     when more input bytes are needed, or negative errno on error.
 * @prepare_extradata: Optional hook to validate/filter cd->extradata before avcodec_open2().
 */
struct ffmpeg_dec_codec_ops {
	enum ffmpeg_dec_codec id;
	uint32_t snd_codec_id;
	const char *name;
	const char *decoder_name;
	uint32_t (*max_frame_samples)(const struct ffmpeg_dec_comp_data *cd);
	int (*skip_container)(struct processing_module *mod, struct sof_source *src);
	void (*prepare_extradata)(struct processing_module *mod);
};

extern const struct ffmpeg_dec_codec_ops ffmpeg_dec_flac_ops;
extern const struct ffmpeg_dec_codec_ops ffmpeg_dec_mp3_ops;
extern const struct ffmpeg_dec_codec_ops ffmpeg_dec_aac_ops;
extern const struct ffmpeg_dec_codec_ops ffmpeg_dec_opus_ops;

/**
 * struct ffmpeg_dec_backend - decode backend operations.
 *
 * A backend owns the real decoder state (allocated by the backend itself and
 * stored in ffmpeg_dec_comp_data.backend_data). All ops return 0 on success or
 * a negative errno.
 */
struct ffmpeg_dec_backend {
	const char *name;

	/* One-time backend init, called from module init(). */
	int (*init)(struct processing_module *mod);

	/* Open/configure the decoder once codec, extradata and the output PCM
	 * format (rate/channels/frame format) are known. Called from prepare().
	 */
	int (*configure)(struct processing_module *mod);

	/*
	 * Decode from @in (in_size bytes of compressed stream) into @out (up to
	 * out_size bytes of interleaved PCM). On return *consumed holds input
	 * bytes eaten and *produced holds PCM bytes written.
	 */
	int (*decode)(struct processing_module *mod,
		      const uint8_t *in, size_t in_size, size_t *consumed,
		      uint8_t *out, size_t out_size, size_t *produced);

	/* Flush decoder state, keep it configured. Called from reset(). */
	int (*reset)(struct processing_module *mod);

	/* Tear down decoder state allocated by init()/configure(). */
	int (*free)(struct processing_module *mod);
};

/**
 * struct ffmpeg_dec_comp_data - ffmpeg_dec module private data.
 * @backend:         Selected decode backend ops.
 * @backend_data:    Backend-private decoder state.
 * @codec_ops:       Per-codec operations for the selected codec.
 * @codec:           Codec id for this instance.
 * @extradata:       Codec setup header (e.g. FLAC STREAMINFO).
 * @extradata_size:  Size of @extradata in bytes (0 if none).
 * @out_rate:        Decoded PCM sample rate (Hz), from the sink stream params.
 * @out_channels:    Decoded PCM channel count.
 * @out_frame_fmt:   Decoded PCM sample format (enum sof_ipc_frame).
 * @out_frame_bytes: Bytes per PCM frame (all channels).
 * @configured:      True once the backend decoder has been opened.
 * @in_buf:          Linear+padded bounce buffer for compressed input.
 * @in_buf_size:     Usable capacity of @in_buf (excludes trailing padding).
 * @pcm_buf:         Linear staging buffer for one decoded frame's PCM.
 * @pcm_buf_size:    Capacity of @pcm_buf.
 * @pcm_avail:       Staged PCM bytes not yet drained into the sink.
 * @pcm_rd:          Read offset of the next byte to drain from @pcm_buf.
 */
struct ffmpeg_dec_comp_data {
	const struct ffmpeg_dec_backend *backend;
	void *backend_data;
	const struct ffmpeg_dec_codec_ops *codec_ops;
	enum ffmpeg_dec_codec codec;
	uint8_t extradata[FFMPEG_DEC_MAX_EXTRADATA];
	size_t extradata_size;
	uint32_t out_rate;
	uint32_t out_channels;
	int out_frame_fmt;
	uint32_t out_frame_bytes;
	bool is_encoder;
	bool configured;
	uint8_t *in_buf;
	size_t in_buf_size;
	uint8_t *pcm_buf;
	size_t pcm_buf_size;
	size_t pcm_avail;
	size_t pcm_rd;
	bool hdr_done;
	size_t skip_bytes;
	bool flac_parsing_meta;
	bool flac_last_block;
	bool draining;
	bool drained;
#if CONFIG_IPC_MAJOR_4
	/*
	 * Compress end-of-stream (drain) support. On a DRAIN trigger the kernel
	 * sets pipeline->expect_eos and blocks in compress_drain() until the
	 * module reports the drain is complete. @eos_msg is the pre-built module
	 * notification sent once the last PCM has been flushed; @eos_sent guards
	 * it against being sent more than once.
	 */
	struct ipc_msg *eos_msg;
	bool eos_sent;
#endif
};

#if CONFIG_IPC_MAJOR_4
struct ipc_msg *ffmpeg_dec_eos_notification_init(struct processing_module *mod);
void ffmpeg_dec_signal_eos(struct processing_module *mod, struct sof_sink *sink);
#endif

/* Backend instance provided by the selected backend translation unit
 * (ffmpeg_dec-stub.c or ffmpeg_dec-ffmpeg.c).
 */
extern const struct ffmpeg_dec_backend ffmpeg_dec_backend;

void ffmpeg_dec_copy_from_circular(void *dst, const void *src,
				   const void *buf_start, size_t buf_size,
				   size_t bytes);

void ffmpeg_dec_copy_to_circular(void *dst, void *buf_start,
				 size_t buf_size, const void *src,
				 size_t bytes);

int ffmpeg_dec_skip_id3v2(struct processing_module *mod, struct sof_source *src);

/**
 * ffmpeg_dec_store_extradata() - Copy a codec setup header into private data.
 * @mod:  Pointer to module data.
 * @data: Setup header bytes (e.g. FLAC STREAMINFO).
 * @size: Number of bytes.
 *
 * Return: Zero if success, otherwise error code.
 */
int ffmpeg_dec_store_extradata(struct processing_module *mod,
			       const uint8_t *data, size_t size);

/**
 * ffmpeg_dec_libc_bind() - Bind the module whose heap backs malloc/free/realloc.
 * @mod: Pointer to module data (see ffmpeg_dec-alloc.c).
 *
 * Called at the entry of every libavcodec backend op so allocations made by
 * FFmpeg are drawn from the current instance's SOF heap.
 */
void ffmpeg_dec_libc_bind(struct processing_module *mod);

/**
 * ffmpeg_dec_heap_free() - Free bytes in the FFmpeg backing heap.
 * Returns 0 for allocator backends without a dedicated heap.
 */
size_t ffmpeg_dec_heap_free(void);
size_t ffmpeg_dec_heap_lastfail(void); /*DBG*/

#endif /* __SOF_AUDIO_FFMPEG_DEC_H__ */
