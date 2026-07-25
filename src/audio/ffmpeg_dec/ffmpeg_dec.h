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
#if defined(CONFIG_FFMPEG_DEC_FLAC)
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
struct ipc_msg;

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
 * @codec:           Codec id for this instance.
 * @extradata:       Codec setup header (e.g. FLAC STREAMINFO), NULL if none.
 * @extradata_size:  Size of @extradata in bytes.
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
	enum ffmpeg_dec_codec codec;
	uint8_t *extradata;
	size_t extradata_size;
	uint32_t out_rate;
	uint32_t out_channels;
	int out_frame_fmt;
	uint32_t out_frame_bytes;
	bool configured;
	uint8_t *in_buf;
	size_t in_buf_size;
	uint8_t *pcm_buf;
	size_t pcm_buf_size;
	size_t pcm_avail;
	size_t pcm_rd;
	bool hdr_done;
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
	/* Filter mode (CONFIG_FFMPEG_DEC_FILTER_MODE): avfilter graph handle
	 * (struct ffmpeg_af_graph *, see ffmpeg_dec-filter.c). Unused for decode.
	 */
	void *af_graph;
};

/* Encode-mode module ops (PCM -> compressed), in ffmpeg_dec-encode.c. */
int ffmpeg_enc_mod_init(struct processing_module *mod);
int ffmpeg_enc_mod_prepare(struct processing_module *mod,
			   struct sof_source **sources, int num_of_sources,
			   struct sof_sink **sinks, int num_of_sinks);
int ffmpeg_enc_mod_process(struct processing_module *mod,
			   struct input_stream_buffer *input_buffers, int num_input_buffers,
			   struct output_stream_buffer *output_buffers, int num_output_buffers);
int ffmpeg_enc_mod_free(struct processing_module *mod);

/* Filter-mode module ops (PCM source/sink effect), in ffmpeg_dec-filter.c. */
int ffmpeg_af_mod_init(struct processing_module *mod);
int ffmpeg_af_mod_prepare(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks);
int ffmpeg_af_mod_process(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks);
int ffmpeg_af_mod_free(struct processing_module *mod);

/* Backend instance provided by the selected backend translation unit
 * (ffmpeg_dec-stub.c or ffmpeg_dec-ffmpeg.c).
 */
extern const struct ffmpeg_dec_backend ffmpeg_dec_backend;

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

#endif /* __SOF_AUDIO_FFMPEG_DEC_H__ */
