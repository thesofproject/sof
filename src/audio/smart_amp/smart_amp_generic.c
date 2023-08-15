// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Maxim Integrated All rights reserved.
//
// Author: Ryan Lee <ryans.lee@maximintegrated.com>
//
// Copyright(c) 2023 Google LLC.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/smart_amp/smart_amp.h>

static void remap_s32_to_s32(struct smart_amp_mod_stream *src_mod, uint32_t frames,
			     const struct audio_stream __sparse_cache *src,
			     const int8_t *chan_map)
{
	int num_samples_remaining;
	int nmax, n, ch, i;
	int n_mod = 0;
	int src_ch = audio_stream_get_channels(src);
	int32_t *src_ptr_base = audio_stream_get_rptr(src);
	int32_t *mod_ptr_base = (int32_t *)src_mod->buf.data;
	int32_t *src_ptr;
	int32_t *mod_ptr;

	/* clean up dst buffer to make sure all 0s on the unmapped channel */
	bzero(mod_ptr_base, src_mod->buf.size);

	num_samples_remaining = frames * src_ch;
	while (num_samples_remaining) {
		nmax = audio_stream_samples_without_wrap_s32(src, src_ptr_base);
		n = MIN(num_samples_remaining, nmax);

		for (ch = 0; ch < src_mod->channels; ch++) {
			if (chan_map[ch] == -1)
				continue;

			mod_ptr = mod_ptr_base + ch;
			src_ptr = src_ptr_base + chan_map[ch];
			n_mod = 0;
			for (i = 0; i < n; i += src_ch) {
				*mod_ptr = *src_ptr;
				mod_ptr += src_mod->channels;
				src_ptr += src_ch;
				n_mod += src_mod->channels;
			}
		}
		/* update base pointers by forwarding (n / src_ch) frames */
		mod_ptr_base += n_mod;
		src_ptr_base += n;

		num_samples_remaining -= n;
		src_ptr_base = audio_stream_wrap(src, src_ptr_base);
	}
}

static void remap_s24_to_s24(struct smart_amp_mod_stream *src_mod, uint32_t frames,
			     const struct audio_stream __sparse_cache *src,
			     const int8_t *chan_map)
{
	remap_s32_to_s32(src_mod, frames, src, chan_map);
}

static void remap_s24_to_s32(struct smart_amp_mod_stream *src_mod, uint32_t frames,
			     const struct audio_stream __sparse_cache *src,
			     const int8_t *chan_map)
{
	int i;
	int n_mod = frames * src_mod->channels;
	int32_t *mod_ptr = (int32_t *)src_mod->buf.data;

	remap_s32_to_s32(src_mod, frames, src, chan_map);

	/* one loop for in-place lshift (s24-to-s32) after remapping */
	for (i = 0; i < n_mod; i++) {
		*mod_ptr = *mod_ptr << 8;
		mod_ptr++;
	}
}

static void remap_s16_to_s16(struct smart_amp_mod_stream *src_mod, uint32_t frames,
			     const struct audio_stream __sparse_cache *src,
			     const int8_t *chan_map)
{
	int num_samples_remaining;
	int nmax, n, ch, i;
	int n_mod = 0;
	int src_ch = audio_stream_get_channels(src);
	int16_t *src_ptr_base = audio_stream_get_rptr(src);
	int16_t *mod_ptr_base = (int16_t *)src_mod->buf.data;
	int16_t *src_ptr;
	int16_t *mod_ptr;

	/* clean up mod buffer (dst) to keep all-0 data on the unmapped channel */
	bzero(mod_ptr_base, src_mod->buf.size);

	num_samples_remaining = frames * src_ch;
	while (num_samples_remaining) {
		nmax = audio_stream_samples_without_wrap_s16(src, src_ptr_base);
		n = MIN(num_samples_remaining, nmax);

		for (ch = 0; ch < src_mod->channels; ch++) {
			if (chan_map[ch] == -1)
				continue;

			mod_ptr = mod_ptr_base + ch;
			src_ptr = src_ptr_base + chan_map[ch];
			n_mod = 0;
			for (i = 0; i < n; i += src_ch) {
				*mod_ptr = *src_ptr;
				mod_ptr += src_mod->channels;
				src_ptr += src_ch;
				n_mod += src_mod->channels;
			}
		}
		/* update base pointers by forwarding (n / src_ch) frames */
		mod_ptr_base += n_mod;
		src_ptr_base += n;

		num_samples_remaining -= n;
		src_ptr_base = audio_stream_wrap(src, src_ptr_base);
	}
}

static void remap_s16_to_b32(struct smart_amp_mod_stream *src_mod, uint32_t frames,
			     const struct audio_stream __sparse_cache *src,
			     const int8_t *chan_map, int lshift)
{
	int num_samples_remaining;
	int nmax, n, ch, i;
	int n_mod = 0;
	int src_ch = audio_stream_get_channels(src);
	int16_t *src_ptr_base = audio_stream_get_rptr(src);
	int32_t *mod_ptr_base = (int32_t *)src_mod->buf.data;
	int16_t *src_ptr;
	int32_t *mod_ptr;

	/* clean up dst buffer to make sure all 0s on the unmapped channel */
	bzero(mod_ptr_base, src_mod->buf.size);

	num_samples_remaining = frames * src_ch;
	while (num_samples_remaining) {
		nmax = audio_stream_samples_without_wrap_s16(src, src_ptr_base);
		n = MIN(num_samples_remaining, nmax);

		for (ch = 0; ch < src_mod->channels; ch++) {
			if (chan_map[ch] == -1)
				continue;

			mod_ptr = mod_ptr_base + ch;
			src_ptr = src_ptr_base + chan_map[ch];
			n_mod = 0;
			for (i = 0; i < n; i += src_ch) {
				*mod_ptr = (int32_t)*src_ptr << lshift;
				mod_ptr += src_mod->channels;
				src_ptr += src_ch;
				n_mod += src_mod->channels;
			}
		}
		/* update base pointers by forwarding (n / src_ch) frames */
		mod_ptr_base += n_mod;
		src_ptr_base += n;

		num_samples_remaining -= n;
		src_ptr_base = audio_stream_wrap(src, src_ptr_base);
	}
}

static void remap_s16_to_s24(struct smart_amp_mod_stream *src_mod, uint32_t frames,
			     const struct audio_stream __sparse_cache *src,
			     const int8_t *chan_map)
{
	remap_s16_to_b32(src_mod, frames, src, chan_map, 8 /* lshift */);
}

static void remap_s16_to_s32(struct smart_amp_mod_stream *src_mod, uint32_t frames,
			     const struct audio_stream __sparse_cache *src,
			     const int8_t *chan_map)
{
	remap_s16_to_b32(src_mod, frames, src, chan_map, 16 /* lshift */);
}

static void feed_s32_to_s32(const struct smart_amp_mod_stream *sink_mod, uint32_t frames,
			    const struct audio_stream __sparse_cache *sink)
{
	int num_samples_remaining;
	int nmax, n, ch, i;
	int sink_ch = audio_stream_get_channels(sink);
	int feed_channels = MIN(sink_ch, sink_mod->channels);
	int32_t *sink_ptr = audio_stream_get_wptr(sink);
	int32_t *mod_ptr = (int32_t *)sink_mod->buf.data;

	num_samples_remaining = frames * sink_ch;
	while (num_samples_remaining) {
		nmax = audio_stream_samples_without_wrap_s32(sink, sink_ptr);
		n = MIN(num_samples_remaining, nmax);

		for (i = 0; i < n; i += sink_ch) {
			for (ch = 0; ch < feed_channels; ch++)
				*(sink_ptr + ch) = *(mod_ptr + ch);

			sink_ptr += sink_ch;
			mod_ptr += sink_mod->channels;
		}

		num_samples_remaining -= n;
		sink_ptr = audio_stream_wrap(sink, sink_ptr);
	}
}

static void feed_s24_to_s24(const struct smart_amp_mod_stream *sink_mod, uint32_t frames,
			    const struct audio_stream __sparse_cache *sink)
{
	feed_s32_to_s32(sink_mod, frames, sink);
}

static void feed_s32_to_s24(const struct smart_amp_mod_stream *sink_mod, uint32_t frames,
			    const struct audio_stream __sparse_cache *sink)
{
	int i;
	int sink_ch = audio_stream_get_channels(sink);
	int n_mod = frames * sink_mod->channels;
	int32_t *mod_ptr = (int32_t *)sink_mod->buf.data;

	/* one loop for in-place rshift (s32-to-s24) before feeding */
	for (i = 0; i < n_mod; i++) {
		*mod_ptr = sat_int24(Q_SHIFT_RND(*mod_ptr, 31, 23));
		mod_ptr++;
	}

	feed_s32_to_s32(sink_mod, frames, sink);
}

static void feed_s16_to_s16(const struct smart_amp_mod_stream *sink_mod, uint32_t frames,
			    const struct audio_stream __sparse_cache *sink)
{
	int num_samples_remaining;
	int nmax, n, ch, i;
	int sink_ch = audio_stream_get_channels(sink);
	int feed_channels = MIN(sink_ch, sink_mod->channels);
	int16_t *sink_ptr = audio_stream_get_wptr(sink);
	int16_t *mod_ptr = (int16_t *)sink_mod->buf.data;

	num_samples_remaining = frames * sink_ch;
	while (num_samples_remaining) {
		nmax = audio_stream_samples_without_wrap_s16(sink, sink_ptr);
		n = MIN(num_samples_remaining, nmax);

		for (i = 0; i < n; i += sink_ch) {
			for (ch = 0; ch < feed_channels; ch++)
				*(sink_ptr + ch) = *(mod_ptr + ch);

			sink_ptr += sink_ch;
			mod_ptr += sink_mod->channels;
		}

		num_samples_remaining -= n;
		sink_ptr = audio_stream_wrap(sink, sink_ptr);
	}
}

static void feed_b32_to_s16(const struct smart_amp_mod_stream *sink_mod, uint32_t frames,
			    const struct audio_stream __sparse_cache *sink, int mod_fbits)
{
	int num_samples_remaining;
	int nmax, n, ch, i;
	int sink_ch = audio_stream_get_channels(sink);
	int feed_channels = MIN(sink_ch, sink_mod->channels);
	int16_t *sink_ptr = audio_stream_get_wptr(sink);
	int32_t *mod_ptr = (int32_t *)sink_mod->buf.data;

	num_samples_remaining = frames * sink_ch;
	while (num_samples_remaining) {
		nmax = audio_stream_samples_without_wrap_s16(sink, sink_ptr);
		n = MIN(num_samples_remaining, nmax);

		for (i = 0; i < n; i += sink_ch) {
			for (ch = 0; ch < feed_channels; ch++) {
				*(sink_ptr + ch) = sat_int16(Q_SHIFT_RND(*(mod_ptr + ch),
									 mod_fbits, 15));
			}
			sink_ptr += sink_ch;
			mod_ptr += sink_mod->channels;
		}

		num_samples_remaining -= n;
		sink_ptr = audio_stream_wrap(sink, sink_ptr);
	}
}

static void feed_s24_to_s16(const struct smart_amp_mod_stream *sink_mod, uint32_t frames,
			    const struct audio_stream __sparse_cache *sink)
{
	feed_b32_to_s16(sink_mod, frames, sink, 23 /* mod_fbits */);
}

static void feed_s32_to_s16(const struct smart_amp_mod_stream *sink_mod, uint32_t frames,
			    const struct audio_stream __sparse_cache *sink)
{
	feed_b32_to_s16(sink_mod, frames, sink, 31 /* mod_fbits */);
}

const struct smart_amp_func_map src_sink_func_map[] = {
	/* { comp_fmt, mod_fmt, src_func, sink_func }
	 * cases are valid only if comp_fmt <= mod_fmt
	 */
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, &remap_s16_to_s16, &feed_s16_to_s16 },

#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, &remap_s16_to_s24, &feed_s24_to_s16 },
#endif  /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, &remap_s16_to_s32, &feed_s32_to_s16 },
#endif  /* CONFIG_FORMAT_S32LE */
#endif  /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, &remap_s24_to_s24, &feed_s24_to_s24 },

#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, &remap_s24_to_s32, &feed_s32_to_s24 },
#endif  /* CONFIG_FORMAT_S32LE */
#endif  /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, &remap_s32_to_s32, &feed_s32_to_s32 },
#endif  /* CONFIG_FORMAT_S32LE */
};

smart_amp_src_func smart_amp_get_src_func(uint16_t comp_fmt, uint16_t mod_fmt)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(src_sink_func_map); i++) {
		if (comp_fmt == src_sink_func_map[i].comp_fmt &&
		    mod_fmt == src_sink_func_map[i].mod_fmt)
			return src_sink_func_map[i].src_func;
	}

	return NULL;
}

smart_amp_sink_func smart_amp_get_sink_func(uint16_t comp_fmt, uint16_t mod_fmt)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(src_sink_func_map); i++) {
		if (comp_fmt == src_sink_func_map[i].comp_fmt &&
		    mod_fmt == src_sink_func_map[i].mod_fmt)
			return src_sink_func_map[i].sink_func;
	}

	return NULL;
}
