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

/* DSM parameter table structure
 * +--------------+-----------------+---------------------------------+
 * | ID (4 bytes) | VALUE (4 bytes) | 1st channel :                   |
 * |              |                 | 8 bytes per single parameter    |
 * +--------------+-----------------+---------------------------------+
 * | ...          | ...             | Repeat N times for N parameters |
 * +--------------+-----------------+---------------------------------+
 * | ID (4 bytes) | VALUE (4 bytes) | 2nd channel :                   |
 * |              |                 | 8 bytes per single parameter    |
 * +--------------+-----------------+---------------------------------+
 * | ...          | ...             | Repeat N times for N parameters |
 * +--------------+-----------------+---------------------------------+
 */
enum dsm_param {
	DSM_PARAM_ID = 0,
	DSM_PARAM_VALUE,
	DSM_PARAM_MAX
};

#define DSM_SINGLE_PARAM_SZ	(DSM_PARAM_MAX * SMART_AMP_FF_MAX_CH_NUM)

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
	int32_t *input;
	/* buffer : feed forward process output */
	int32_t *output;
	/* buffer : feedback voltage */
	int32_t *voltage;
	/* buffer : feedback current */
	int32_t *current;
	/* buffer : feed forward variable length -> fixed length */
	struct smart_amp_ff_buf_struct_t ff;
	/* buffer : feed forward variable length <- fixed length */
	struct smart_amp_ff_buf_struct_t ff_out;
	/* buffer : feedback variable length -> fixed length */
	struct smart_amp_fb_buf_struct_t fb;
};

struct param_buf_struct_t {
	int id;
	int value;
};

struct smart_amp_caldata {
	uint32_t data_size;			/* size of component's model data */
	void *data;				/* model data pointer */
	uint32_t data_pos;			/* data position for read/write */
};

struct smart_amp_param_struct_t {
	struct param_buf_struct_t param;	/* variable to keep last parameter ID/value */
	struct smart_amp_caldata caldata;	/* model data buffer */
	int pos;				/* data position for read/write */
	int max_param;				/* keep max number of DSM parameters */
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
	/* Channel bit dempth */
	int bitwidth;
	struct smart_amp_param_struct_t param;
};

typedef void (*smart_amp_func)(const struct comp_dev *dev,
			       const struct audio_stream __sparse_cache *source,
			       const struct audio_stream __sparse_cache *sink,
			       const struct audio_stream __sparse_cache *feedback,
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
		      const struct audio_stream __sparse_cache *source,
		      const struct audio_stream __sparse_cache *sink, int8_t *chan_map,
		      struct smart_amp_mod_struct_t *hspk,
		      uint32_t num_ch_in, uint32_t num_ch_out);
/* Feedback processing function */
int smart_amp_fb_copy(struct comp_dev *dev, uint32_t frames,
		      const struct audio_stream __sparse_cache *source,
		      const struct audio_stream __sparse_cache *sink, int8_t *chan_map,
		      struct smart_amp_mod_struct_t *hspk,
		      uint32_t num_ch);
/* memory usage calculation for the component */
int smart_amp_get_memory_size(struct smart_amp_mod_struct_t *hspk,
			      struct comp_dev *dev);
/* supported audio format check */
int smart_amp_check_audio_fmt(int sample_rate, int ch_num);

/* this function return number of parameters supported */
int smart_amp_get_num_param(struct smart_amp_mod_struct_t *hspk,
			    struct comp_dev *dev);
/* this function update whole parameter table */
int smart_amp_get_all_param(struct smart_amp_mod_struct_t *hspk,
			    struct comp_dev *dev);
/* parameter read function */
int maxim_dsm_get_param(struct smart_amp_mod_struct_t *hspk,
			struct comp_dev *dev,
			struct sof_ipc_ctrl_data *cdata, int size);
/* parameter write function */
int maxim_dsm_set_param(struct smart_amp_mod_struct_t *hspk,
			struct comp_dev *dev,
			struct sof_ipc_ctrl_data *cdata);
/* parameter restoration */
int maxim_dsm_restore_param(struct smart_amp_mod_struct_t *hspk,
			    struct comp_dev *dev);
/* parameter forced read, ignore cache */
int maxim_dsm_get_param_forced(struct smart_amp_mod_struct_t *hspk,
			       struct comp_dev *dev);
#endif
