// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Maxim Integrated All rights reserved.
//
// Author: Ryan Lee <ryans.lee@maximintegrated.com>

#include <sof/audio/component.h>
#include <sof/trace/trace.h>
#include <sof/drivers/ipc.h>
#include <sof/ut.h>

#include <sof/audio/channel_map.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <sof/bit.h>
#include <sof/common.h>
#include <ipc/channel_map.h>
#include <stdint.h>
#include <stdlib.h>
#include <sof/audio/smart_amp/smart_amp.h>
#include "dsm_api_public.h"

static int maxim_dsm_init(struct smart_amp_mod_struct_t *hspk, struct comp_dev *dev)
{
	int *circularbuffersize = hspk->circularbuffersize;
	int *delayedsamples = hspk->delayedsamples;
	struct dsm_api_init_ext_t initparam;
	enum DSM_API_MESSAGE retcode;

	comp_dbg(dev, BUILD_TIME);

	initparam.isamplebitwidth       = DSM_DEFAULT_CH_SIZE;
	initparam.ichannels             = DSM_DEFAULT_NUM_CHANNEL;
	initparam.ipcircbuffersizebytes = circularbuffersize;
	initparam.ipdelayedsamples      = delayedsamples;
	initparam.isamplingrate         = DSM_DEFAULT_SAMPLE_RATE;

	retcode = dsm_api_init(hspk->dsmhandle, &initparam,
			       sizeof(struct dsm_api_init_ext_t));
	if (retcode != DSM_API_OK) {
		goto exit;
	} else	{
		hspk->ff_fr_sz_samples =
			initparam.off_framesizesamples;
		hspk->fb_fr_sz_samples =
			initparam.ofb_framesizesamples;
		hspk->channelmask = 0;
		hspk->nchannels = initparam.ichannels;
		hspk->ifsamples = hspk->ff_fr_sz_samples
			* initparam.ichannels;
		hspk->ibsamples = hspk->fb_fr_sz_samples
			* initparam.ichannels;
	}

	comp_dbg(dev, "[DSM] Initialization completed. (module:%p, dsm:%p)",
		 (uintptr_t)hspk,
		 (uintptr_t)hspk->dsmhandle);

	return 0;
exit:
	comp_err(dev, "[DSM] Initialization failed. ret:%d", retcode);
	return (int)retcode;
}

static void maxim_dsm_ff_proc(struct smart_amp_mod_struct_t *hspk,
			      struct comp_dev *dev, void *in, void *out,
			      int nsamples, int szsample)
{
	union smart_amp_buf buf, buf_out;
	int16_t *input = (int16_t *)hspk->buf.input;
	int16_t *output = (int16_t *)hspk->buf.output;
	int *w_ptr = &hspk->buf.ff.avail;
	int *r_ptr = &hspk->buf.ff_out.avail;
	bool is_16bit = szsample == 2 ? true : false;
	int remain;
	int x;

	buf.buf16 = (int16_t *)hspk->buf.ff.buf;
	buf.buf32 = (int32_t *)hspk->buf.ff.buf;
	buf_out.buf16 = (int16_t *)hspk->buf.ff_out.buf;
	buf_out.buf32 = (int32_t *)hspk->buf.ff_out.buf;

	/* Current pointer(w_ptr) + number of input frames(nsamples)
	 * must be smaller than buffer size limit
	 */
	if (*w_ptr + nsamples <= DSM_FF_BUF_DB_SZ) {
		if (is_16bit)
			memcpy_s(&buf.buf16[*w_ptr], nsamples * szsample,
				 in, nsamples * szsample);
		else
			memcpy_s(&buf.buf32[*w_ptr], nsamples * szsample,
				 in, nsamples * szsample);
		*w_ptr += nsamples;
	} else {
		comp_err(dev,
			 "[DSM] Feed Forward buffer overflow. (w_ptr : %d + %d > %d)",
			 *w_ptr, nsamples, DSM_FF_BUF_DB_SZ);
		return;
	}

	/* Run DSM Feedforward process if the buffer is ready */
	if (*w_ptr >= DSM_FF_BUF_SZ) {
		if (is_16bit) {
			/* Buffer ordering for DSM : LRLR... -> LL...RR... */
			for (x = 0; x < DSM_FRM_SZ; x++) {
				input[x] = (buf.buf16[2 * x]);
				input[x + DSM_FRM_SZ] =
					(buf.buf16[2 * x + 1]);
			}
		} else {
			for (x = 0; x < DSM_FRM_SZ; x++) {
				input[x] = (buf.buf32[2 * x] >> 16);
				input[x + DSM_FRM_SZ] =
					(buf.buf32[2 * x + 1] >> 16);
			}
		}

		remain = (*w_ptr - DSM_FF_BUF_SZ);
		if (remain) {
			if (is_16bit)
				memcpy_s(&buf.buf16[0], remain * szsample,
					 &buf.buf16[DSM_FF_BUF_SZ],
					 remain * szsample);
			else
				memcpy_s(&buf.buf32[0], remain * szsample,
					 &buf.buf32[DSM_FF_BUF_SZ],
					 remain * szsample);
		}
		*w_ptr -= DSM_FF_BUF_SZ;

		hspk->ifsamples = hspk->nchannels * hspk->ff_fr_sz_samples;
		dsm_api_ff_process(hspk->dsmhandle, hspk->channelmask,
				   input, &hspk->ifsamples,
				   output, &hspk->ofsamples);

		if (is_16bit) {
			for (x = 0; x < DSM_FRM_SZ; x++) {
				/* Buffer re-ordering LR/LR/LR */
				buf_out.buf16[*r_ptr + 2 * x] = (output[x]);
				buf_out.buf16[*r_ptr + 2 * x + 1] =
					(output[x + DSM_FRM_SZ]);
			}
		} else {
			for (x = 0; x < DSM_FRM_SZ; x++) {
				buf_out.buf32[*r_ptr + 2 * x] =
					(output[x] << 16);
				buf_out.buf32[*r_ptr + 2 * x + 1] =
					(output[x + DSM_FRM_SZ] << 16);
			}
		}

		*r_ptr += DSM_FF_BUF_SZ;
	}

	/* Output buffer preparation */
	if (*r_ptr >= nsamples) {
		memcpy_s(out, nsamples * szsample,
			 buf_out.buf16, nsamples * szsample);
		remain = (*r_ptr - nsamples);
		if (remain) {
			if (is_16bit)
				memcpy_s(&buf_out.buf16[0], remain * szsample,
					 &buf_out.buf16[nsamples],
					 remain * szsample);
			else
				memcpy_s(&buf_out.buf32[0], remain * szsample,
					 &buf_out.buf32[nsamples],
					 remain * szsample);
		}
		*r_ptr -= nsamples;
	} else {
		memset(out, 0, nsamples * szsample);
		comp_err(dev,
			 "[DSM] DSM FF process underrun. r_ptr : %d",
			 *r_ptr);
	}
}

static void maxim_dsm_fb_proc(struct smart_amp_mod_struct_t *hspk,
			      struct comp_dev *dev, void *in,
			      int nsamples, int szsample)
{
	union smart_amp_buf buf;
	int *w_ptr = &hspk->buf.fb.avail;
	int16_t *v = hspk->buf.voltage;
	int16_t *i = hspk->buf.current;
	bool is_16bit = szsample == 2 ? true : false;
	int remain;
	int x;

	buf.buf16 = (int16_t *)hspk->buf.fb.buf;
	buf.buf32 = (int32_t *)hspk->buf.fb.buf;

	/* Current pointer(w_ptr) + number of input frames(nsamples)
	 * must be smaller than buffer size limit
	 */
	if (*w_ptr + nsamples <= DSM_FB_BUF_DB_SZ) {
		if (is_16bit)
			memcpy_s(&buf.buf16[*w_ptr], nsamples * szsample,
				 in, nsamples * szsample);
		else
			memcpy_s(&buf.buf32[*w_ptr], nsamples * szsample,
				 in, nsamples * szsample);
		*w_ptr += nsamples;
	} else {
		comp_err(dev, "[DSM] Feedback buffer overflow. w_ptr : %d",
			 *w_ptr);
		return;
	}
	/* Run DSM Feedback process if the buffer is ready */
	if (*w_ptr >= DSM_FB_BUF_SZ) {
		if (is_16bit) {
			for (x = 0; x < DSM_FRM_SZ; x++) {
			/* Buffer ordering for DSM : VIVI... -> VV... II...*/
				v[x] = buf.buf16[4 * x];
				i[x] = buf.buf16[4 * x + 1];
				v[x + DSM_FRM_SZ] = buf.buf16[4 * x + 2];
				i[x + DSM_FRM_SZ] = buf.buf16[4 * x + 3];
			}
		} else {
			for (x = 0; x < DSM_FRM_SZ; x++) {
				v[x] = (buf.buf32[4 * x] >> 16);
				i[x] = (buf.buf32[4 * x + 1] >> 16);
				v[x + DSM_FRM_SZ] =
					(buf.buf32[4 * x + 2] >> 16);
				i[x + DSM_FRM_SZ] =
					(buf.buf32[4 * x + 3] >> 16);
			}
		}

		remain = (*w_ptr - DSM_FB_BUF_SZ);
		if (remain) {
			if (is_16bit)
				memcpy_s(&buf.buf16[0], remain * szsample,
					 &buf.buf16[DSM_FB_BUF_SZ],
					 remain * szsample);
			else
				memcpy_s(&buf.buf32[0], remain * szsample,
					 &buf.buf32[DSM_FB_BUF_SZ],
					 remain * szsample);
		}
		*w_ptr -= DSM_FB_BUF_SZ;

		hspk->ibsamples = hspk->fb_fr_sz_samples * hspk->nchannels;
		dsm_api_fb_process(hspk->dsmhandle,
				   hspk->channelmask,
				   i, v, &hspk->ibsamples);

	}
}

int smart_amp_flush(struct smart_amp_mod_struct_t *hspk, struct comp_dev *dev)
{
	memset(hspk->buf.frame_in, 0,
	       SMART_AMP_FF_BUF_DB_SZ * sizeof(int32_t));
	memset(hspk->buf.frame_out, 0,
	       SMART_AMP_FF_BUF_DB_SZ * sizeof(int32_t));
	memset(hspk->buf.frame_iv, 0,
	       SMART_AMP_FB_BUF_DB_SZ * sizeof(int32_t));

	memset(hspk->buf.input, 0, DSM_FF_BUF_SZ * sizeof(int16_t));
	memset(hspk->buf.output, 0, DSM_FF_BUF_SZ * sizeof(int16_t));
	memset(hspk->buf.voltage, 0, DSM_FF_BUF_SZ * sizeof(int16_t));
	memset(hspk->buf.current, 0, DSM_FF_BUF_SZ * sizeof(int16_t));

	memset(hspk->buf.ff.buf, 0, DSM_FF_BUF_DB_SZ * sizeof(int32_t));
	memset(hspk->buf.ff_out.buf, 0, DSM_FF_BUF_DB_SZ * sizeof(int32_t));
	memset(hspk->buf.fb.buf, 0, DSM_FB_BUF_DB_SZ * sizeof(int32_t));

	hspk->buf.ff.avail = DSM_FF_BUF_SZ;
	hspk->buf.ff_out.avail = 0;
	hspk->buf.fb.avail = 0;

	comp_dbg(dev, "[DSM] Reset (handle:%p)", (uintptr_t)hspk);

	return 0;
}

int smart_amp_init(struct smart_amp_mod_struct_t *hspk, struct comp_dev *dev)
{
	return maxim_dsm_init(hspk, dev);
}

int smart_amp_get_memory_size(struct smart_amp_mod_struct_t *hspk,
			      struct comp_dev *dev)
{
	enum DSM_API_MESSAGE retcode;
	struct dsm_api_memory_size_ext_t memsize;
	int *circularbuffersize = hspk->circularbuffersize;

	memsize.ichannels = DSM_DEFAULT_NUM_CHANNEL;
	memsize.ipcircbuffersizebytes = circularbuffersize;
	memsize.isamplingrate = DSM_DEFAULT_SAMPLE_RATE;
	memsize.omemsizerequestedbytes = 0;
	memsize.numeqfilters = DSM_DEFAULT_NUM_EQ;
	retcode = dsm_api_get_mem(&memsize,
				  sizeof(struct dsm_api_memory_size_ext_t));
	if (retcode != DSM_API_OK)
		return 0;

	return memsize.omemsizerequestedbytes;
}

int smart_amp_check_audio_fmt(int sample_rate, int ch_num)
{
	/* Return error if the format is not supported by DSM component */
	if (sample_rate != DSM_DEFAULT_SAMPLE_RATE)
		return -EINVAL;
	if (ch_num > DSM_DEFAULT_NUM_CHANNEL)
		return -EINVAL;

	return 0;
}

static int smart_amp_get_buffer(int32_t *buf, uint32_t frames,
				struct audio_stream *stream,
				int8_t *chan_map, uint32_t num_ch)
{
	int x, y;
	uint32_t in_frag = 0;
	union smart_amp_buf input, output;
	int index;

	input.buf16 = (int16_t *)stream->r_ptr;
	input.buf32 = (int32_t *)stream->r_ptr;
	output.buf16 = (int16_t *)buf;
	output.buf32 = (int32_t *)buf;

	switch (stream->frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		for (x = 0 ; x < frames ; x++) {
			for (y = 0 ; y < num_ch; y++) {
				if (chan_map[y] == -1)
					continue;
				index = in_frag + chan_map[y];
				input.buf16 =
					audio_stream_read_frag_s16(stream,
								   index);
				output.buf16[num_ch * x + y] = *input.buf16;
			}
			in_frag += stream->channels;
		}
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		for (x = 0 ; x < frames ; x++) {
			for (y = 0 ; y < num_ch ; y++) {
				if (chan_map[y] == -1)
					continue;
				index = in_frag + chan_map[y];
				input.buf32 =
					audio_stream_read_frag_s32(stream,
								   index);
				output.buf32[num_ch * x + y] = *input.buf32;
			}
			in_frag += stream->channels;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int smart_amp_put_buffer(int32_t *buf, uint32_t frames,
				struct audio_stream *stream,
				int8_t *chan_map, uint32_t num_ch_in,
				uint32_t num_ch_out)
{
	union smart_amp_buf input, output;
	uint32_t out_frag = 0;
	int x, y;

	input.buf16 = (int16_t *)buf;
	input.buf32 = (int32_t *)buf;
	output.buf16 = (int16_t *)stream->w_ptr;
	output.buf32 = (int32_t *)stream->w_ptr;

	switch (stream->frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		for (x = 0 ; x < frames ; x++) {
			for (y = 0 ; y < num_ch_out; y++) {
				if (chan_map[y] == -1) {
					out_frag++;
					continue;
				}
				output.buf16 =
					audio_stream_write_frag_s16(stream,
								    out_frag);
				*output.buf16 = input.buf16[num_ch_in * x + y];
				out_frag++;
			}
		}
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		for (x = 0 ; x < frames ; x++) {
			for (y = 0 ; y < num_ch_out; y++) {
				if (chan_map[y] == -1) {
					out_frag++;
					continue;
				}
				output.buf32 =
					audio_stream_write_frag_s32(stream,
								    out_frag);
				*output.buf32 = input.buf32[num_ch_in * x + y];
				out_frag++;
			}
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int smart_amp_ff_copy(struct comp_dev *dev, uint32_t frames,
		      struct comp_buffer *source,
		      struct comp_buffer *sink, int8_t *chan_map,
		      struct smart_amp_mod_struct_t *hspk,
		      uint32_t num_ch_in, uint32_t num_ch_out)
{
	int ret;

	if (frames == 0) {
		comp_dbg(dev, "[DSM] feed forward frame size zero warning.");
		return 0;
	}

	if (frames > SMART_AMP_FF_BUF_DB_SZ) {
		comp_err(dev, "[DSM] feed forward frame size overflow  : %d",
			 frames);
		return -EINVAL;
	}

	num_ch_in = MIN(num_ch_in, SMART_AMP_FF_MAX_CH_NUM);
	num_ch_out = MIN(num_ch_out, SMART_AMP_FF_OUT_MAX_CH_NUM);

	ret = smart_amp_get_buffer(hspk->buf.frame_in,
				   frames, &source->stream, chan_map,
				   num_ch_in);
	if (ret)
		goto err;

	switch (source->stream.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		maxim_dsm_ff_proc(hspk, dev,
				  hspk->buf.frame_in,
				  hspk->buf.frame_out,
				  frames * num_ch_in, sizeof(int16_t));
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		maxim_dsm_ff_proc(hspk, dev,
				  hspk->buf.frame_in,
				  hspk->buf.frame_out,
				  frames * num_ch_in, sizeof(int32_t));
		break;
	default:
		ret = -EINVAL;
		goto err;
	}

	ret = smart_amp_put_buffer(hspk->buf.frame_out,
				   frames, &sink->stream, chan_map,
				   MIN(num_ch_in, SMART_AMP_FF_MAX_CH_NUM),
				   MIN(num_ch_out, SMART_AMP_FF_OUT_MAX_CH_NUM));
	if (ret)
		goto err;

	return 0;
err:
	comp_err(dev, "[DSM] Not supported frame format");
	return ret;
}

int smart_amp_fb_copy(struct comp_dev *dev, uint32_t frames,
		      struct comp_buffer *source,
		      struct comp_buffer *sink, int8_t *chan_map,
		      struct smart_amp_mod_struct_t *hspk,
		      uint32_t num_ch)
{
	int ret;

	if (frames == 0) {
		comp_dbg(dev, "[DSM] feedback frame size zero warning.");
		return 0;
	}

	if (frames > SMART_AMP_FB_BUF_DB_SZ) {
		comp_err(dev, "[DSM] feedback frame size overflow  : %d",
			 frames);
		return -EINVAL;
	}

	num_ch = MIN(num_ch, SMART_AMP_FB_MAX_CH_NUM);

	ret = smart_amp_get_buffer(hspk->buf.frame_iv,
				   frames, &source->stream,
				   chan_map, num_ch);
	if (ret)
		goto err;

	switch (source->stream.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		maxim_dsm_fb_proc(hspk, dev, hspk->buf.frame_iv,
				  frames * num_ch, sizeof(int16_t));
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		maxim_dsm_fb_proc(hspk, dev, hspk->buf.frame_iv,
				  frames * num_ch, sizeof(int32_t));
		break;
	default:
		ret = -EINVAL;
		goto err;
	}
	return 0;
err:
	comp_err(dev, "[DSM] Not supported frame format : %d",
		 source->stream.frame_fmt);
	return ret;
}
