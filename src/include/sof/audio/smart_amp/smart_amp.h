/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Maxim Integrated. All rights reserved.
 *
 * Author: Ryan Lee <ryans.lee@maximintegrated.com>
 */

#ifndef __SOF_AUDIO_DSM_H__
#define __SOF_AUDIO_DSM_H__

#include <sof/platform.h>
#include <sof/audio/component.h>

/* Maximum number of channels for algorithm in */
#define SMART_AMP_FF_MAX_CH_NUM		2
/* Maximum number of channels for algorithm out */
#define SMART_AMP_FF_OUT_MAX_CH_NUM	4
/* Maximum number of channels for feedback  */
#define SMART_AMP_FB_MAX_CH_NUM		4

#define SMART_AMP_FRM_SZ	48 /* samples per 1ms */
#define SMART_AMP_FF_BUF_SZ	(SMART_AMP_FRM_SZ * SMART_AMP_FF_MAX_CH_NUM)
#define SMART_AMP_FB_BUF_SZ	(SMART_AMP_FRM_SZ * SMART_AMP_FB_MAX_CH_NUM)

/* Maxim DSM(Dynamic Speaker Manangement) process buffer size */
#define DSM_FRM_SZ		48
#define DSM_FF_BUF_SZ		(DSM_FRM_SZ * SMART_AMP_FF_MAX_CH_NUM)
#define DSM_FB_BUF_SZ		(DSM_FRM_SZ * SMART_AMP_FB_MAX_CH_NUM)

#define SMART_AMP_FF_BUF_DB_SZ\
	(SMART_AMP_FF_BUF_SZ * SMART_AMP_FF_MAX_CH_NUM)
#define SMART_AMP_FB_BUF_DB_SZ\
	(SMART_AMP_FB_BUF_SZ * SMART_AMP_FB_MAX_CH_NUM)
#define DSM_FF_BUF_DB_SZ	(DSM_FF_BUF_SZ * SMART_AMP_FF_MAX_CH_NUM)
#define DSM_FB_BUF_DB_SZ	(DSM_FB_BUF_SZ * SMART_AMP_FB_MAX_CH_NUM)

union smart_amp_buf {
	int16_t *buf16;
	int32_t *buf32;
};

struct smart_amp_ff_buf_struct_t {
	int32_t *buf;
	int avail;
};

struct smart_amp_fb_buf_struct_t {
	int32_t *buf;
	int avail;
	int rdy;
};

struct smart_amp_buf_struct_t {
	/* buffer : sof -> spk protection feed forward process */
	int32_t *frame_in;
	/* buffer : sof <- spk protection feed forward process */
	int32_t *frame_out;
	/* buffer : sof -> spk protection feedback process */
	int32_t *frame_iv;
	/* buffer : feed forward process input */
	int16_t *input;
	/* buffer : feed forward process output */
	int16_t *output;
	/* buffer : feedback voltage */
	int16_t *voltage;
	/* buffer : feedback current */
	int16_t *current;
	/* buffer : feed forward variable length -> fixed length */
	struct smart_amp_ff_buf_struct_t ff;
	/* buffer : feed forward variable length <- fixed length */
	struct smart_amp_ff_buf_struct_t ff_out;
	/* buffer : feedback variable length -> fixed length */
	struct smart_amp_fb_buf_struct_t fb;
};

struct smart_amp_mod_struct_t {
	struct smart_amp_buf_struct_t buf;
	void *dsmhandle;
	/* DSM variables for the initialization */
	int delayedsamples[SMART_AMP_FF_MAX_CH_NUM << 2];
	int circularbuffersize[SMART_AMP_FF_MAX_CH_NUM << 2];
	/* Number of samples of feed forward and feedback frame */
	int ff_fr_sz_samples;
	int fb_fr_sz_samples;
	int channelmask;
	/* Number of channels of DSM */
	int nchannels;
	/* Number of samples of feed forward channel */
	int ifsamples;
	/* Number of samples of feedback channel */
	int ibsamples;
	/* Number of processed samples */
	int ofsamples;
};

typedef void (*smart_amp_func)(const struct comp_dev *dev,
			       const struct audio_stream *source,
			       const struct audio_stream *sink,
			       const struct audio_stream *feedback,
			       uint32_t frames);
struct smart_amp_func_map {
	uint16_t frame_fmt;
	smart_amp_func func;
};

/* Component initialization */
int smart_amp_init(struct smart_amp_mod_struct_t *hspk, struct comp_dev *dev);
/* Component memory flush */
int smart_amp_flush(struct smart_amp_mod_struct_t *hspk, struct comp_dev *dev);
/* Feed forward processing function */
int smart_amp_ff_copy(struct comp_dev *dev, uint32_t frames,
		      struct comp_buffer *source,
		      struct comp_buffer *sink, int8_t *chan_map,
		      struct smart_amp_mod_struct_t *hspk,
		      uint32_t num_ch_in, uint32_t num_ch_out);
/* Feedback processing function */
int smart_amp_fb_copy(struct comp_dev *dev, uint32_t frames,
		      struct comp_buffer *source,
		      struct comp_buffer *sink, int8_t *chan_map,
		      struct smart_amp_mod_struct_t *hspk,
		      uint32_t num_ch);
/* memory usage calculation for the component */
int smart_amp_get_memory_size(struct smart_amp_mod_struct_t *hspk,
			      struct comp_dev *dev);
/* supported audio format check */
int smart_amp_check_audio_fmt(int sample_rate, int ch_num);

#endif
