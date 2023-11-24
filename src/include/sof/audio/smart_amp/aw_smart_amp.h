/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Awinic Integrated. All rights reserved.
 *
 * Author: Jimmy Zhang <zhangjianming@awinic.com>
 */

#ifndef __SOF_AUDIO_AW_H__
#define __SOF_AUDIO_AW_H__

#include <sof/platform.h>
#include <sof/audio/component.h>

/* Awinic Maximum number of channels for algorithm in */
#define SMART_AMP_FF_MAX_CH_NUM		2
/* Awinic Maximum number of channels for algorithm out */
#define SMART_AMP_FF_OUT_MAX_CH_NUM	4
/* Awinic Maximum number of channels for feedback  */
#define SMART_AMP_FB_MAX_CH_NUM		2
/* Awinic cache buffer size (10ms)  */
#define SMART_AMP_BUF_TIME          10

#define SMART_AMP_FRM_SZ	48 /* samples per 1ms */
#define SMART_AMP_FF_BUF_SZ	(SMART_AMP_FRM_SZ * SMART_AMP_FF_MAX_CH_NUM)
#define SMART_AMP_FB_BUF_SZ	(SMART_AMP_FRM_SZ * SMART_AMP_FB_MAX_CH_NUM)

/* Awinic SKune protect process buffer size */
#define SMART_AMP_FF_BUF_DB_SZ\
	(SMART_AMP_FF_BUF_SZ * SMART_AMP_BUF_TIME)
#define SMART_AMP_FB_BUF_DB_SZ\
	(SMART_AMP_FB_BUF_SZ * SMART_AMP_BUF_TIME)


typedef struct media_info {
	uint32_t num_channel;
	uint32_t bit_per_sample;
	uint32_t bit_qactor_sample;
	uint32_t sample_rate;
	uint32_t data_is_signed;
} media_info_t;

typedef struct smart_amp_buf {
	char *data_ptr;
	uint32_t actual_data_len;
	uint32_t max_data_len;
}smart_amp_buf_t;


enum {
	DSP_MSG_TYPE_DATA = 0,
	DSP_MSG_TYPE_CMD  = 1,
};

enum {
	AW_MSG_ID_0 = 0,
	AW_MSG_ID_1 = 1,
};

#define DSP_MSG_VERSION (0x00000001)
enum {
	AW_MESG_NONE = 0,
	AW_MESG_READY = 1,
};

typedef struct {
	int status;
	int32_t opcode_id;
}dsp_cmd_info_t;

typedef struct {
	int32_t type;
	int32_t opcode_id;
	int32_t version;
	int32_t reserver[3];
}dsp_msg_hdr_t;

typedef struct sktune_struct{
	/* buffer : sof -> frame in */
	smart_amp_buf_t frame_in;
	/* buffer : sof <- frame out */
	smart_amp_buf_t frame_out;
	/* buffer : sof -> frame_iv */
	smart_amp_buf_t frame_iv;
	uint32_t enable;
	//dsp_cmd_info_t msg_info;
	dsp_cmd_info_t sub_msg_info[2];
	media_info_t media_info;
	int bitwidth;
	void *handle;
}sktune_t;


/* Component initialization */
int smart_amp_init(sktune_t *      sktune, struct comp_dev *dev);

int smart_amp_deinit(sktune_t *       sktune, struct comp_dev *dev);

/* Component memory flush */
int smart_amp_flush(sktune_t *sktune, struct comp_dev *dev);

/* supported audio format check */
int smart_amp_check_audio_fmt(int sample_rate, int ch_num);

/* parameter read function */
int smart_amp_get_param(sktune_t *sktune,
			struct comp_dev *dev,
			struct sof_ipc_ctrl_data *cdata, int max_size, uint32_t params_id);

/* parameter write function */
int smart_amp_set_param(sktune_t *sktune,
			struct comp_dev *dev,
			struct sof_ipc_ctrl_data *cdata, uint32_t params_id);

sktune_t* smart_amp_sktune_alloc(struct comp_dev *dev);


int smart_amp_fb_data_prepare(sktune_t *       sktune, struct comp_dev *dev,
				const struct audio_stream __sparse_cache *source,
			     uint32_t frames);

int smart_amp_ff_data_prepare(sktune_t *       sktune, struct comp_dev *dev,
				const struct audio_stream __sparse_cache *source,
			     uint32_t frames);

int smart_amp_process(sktune_t *       sktune, struct comp_dev *dev,
				const struct audio_stream __sparse_cache *source,
				const struct audio_stream __sparse_cache *sink,
			     uint32_t frames, int num_ch_out);

#endif
