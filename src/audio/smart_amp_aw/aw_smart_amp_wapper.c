// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 awinic Integrated All rights reserved.
//
// Author: Jimmy Zhang <zhangjianming@awinic.com>


#include <sof/audio/component.h>
#include <sof/trace/trace.h>
#include <sof/ipc/msg.h>
#include <sof/ut.h>

#include <sof/audio/channel_map.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <rtos/bit.h>
#include <sof/common.h>
#include <ipc/channel_map.h>
#include <stdint.h>
#include <stdlib.h>
#include <sof/audio/smart_amp/aw_smart_amp.h>
#include "aw_api_public.h"


#define SOF_SMART_AMP_RX_ENABLE   	(0x10013D11)
#define SOF_SMART_AMP_RX_PARAMS   	(0x10013D02)
//#define SOF_SMART_AMP_TX_ENABLE   	(0x10013D13)
#define SOF_SMART_AMP_RX_VMAX_L   	(0x10013D17)
#define SOF_SMART_AMP_RX_VMAX_R   	(0x10013D18)
#define SOF_SMART_AMP_RX_CALI_CFG_L (0x10013D19)
#define SOF_SMART_AMP_RX_CALI_CFG_R (0x10013D1A)
#define SOF_SMART_AMP_RX_RE_L       (0x10013D1B)
#define SOF_SMART_AMP_RX_RE_R       (0x10013D1C)
#define SOF_SMART_AMP_RX_NOISE_L    (0x10013D1D)
#define SOF_SMART_AMP_RX_NOISE_R    (0x10013D1E)
#define SOF_SMART_AMP_RX_F0_L       (0x10013D1F)
#define SOF_SMART_AMP_RX_F0_R       (0x10013D20)
#define SOF_SMART_AMP_RX_REAL_DATA_L (0x10013D21)
#define SOF_SMART_AMP_RX_REAL_DATA_R (0x10013D22)
#define SOF_SMART_AMP_RX_SKT_PARAMS  (0x10013D25)
#define SOF_SMART_AMP_RX_MSG         (0x10013D2A)
#define SOF_SMART_AMP_RX_SPIN        (0x10013D2E)
//#define SOF_SMART_AMP_RX_SCENE       (0x10013D22)


static int smartpa_amp_get_msg_parser(sktune_t *sktune, struct comp_dev *dev, char *data_buf, uint32_t len, int msg_id)
{
	uint32_t actual_data_len = len;
	char *data_ptr = data_buf;
	int ret;

	if (sktune->sub_msg_info[msg_id].status != AW_MESG_READY) {
		comp_err(dev, "[Awinic] set msg cmd no read, please write cmd first");
		return -1;		
	}

	ret = sktune_api_get_data(sktune->handle, sktune->sub_msg_info[msg_id].opcode_id, data_ptr, actual_data_len);
	if (ret < 0) {
		comp_err(dev, "[Awinic] get msg opcode[0x%x] failed", sktune->sub_msg_info[msg_id].opcode_id);
		sktune->sub_msg_info[msg_id].status = AW_MESG_NONE;
		sktune->sub_msg_info[msg_id].opcode_id = 0;
		return -1;
	}
	comp_info(dev, "[Awinic] get msg opcode[0x%x] done", sktune->sub_msg_info[msg_id].opcode_id);

	sktune->sub_msg_info[msg_id].status = AW_MESG_NONE;
	sktune->sub_msg_info[msg_id].opcode_id = 0;

	return 0;
}

int smart_amp_get_param(sktune_t *sktune,
			struct comp_dev *dev,
			struct sof_ipc_ctrl_data *cdata, int max_size, uint32_t params_id)
{
	size_t buf_size;
	int ret = 0;
	int32_t *data_buf = (int32_t *)ASSUME_ALIGNED(&cdata->data->data, sizeof(uint32_t));

	buf_size = cdata->num_elems;
	
	/* return an error in case of mismatch between num_elems and
	 * required size
	 */
	if (buf_size > max_size) {
		comp_err(dev, "[Awinic] smart_amp_get_param(): invalid num_elems %d, size %d", max_size, buf_size);
		return -1;
	}

	switch (params_id) {
	case SOF_SMART_AMP_RX_ENABLE : {
		if (max_size >= sizeof(int32_t)) {
			 data_buf[0] = sktune->enable;
			 buf_size = sizeof(uint32_t);
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", max_size);
			ret = -1;
		}
	} break;
	case SOF_SMART_AMP_RX_SKT_PARAMS : {
		ret = sktune_api_get_params(sktune->handle, (char *)data_buf, max_size);
		if (ret < 0) {
			comp_err(dev, "[Awinic] get params failed ");
		} else {
			buf_size = ret;
		}
	} break;
	case SOF_SMART_AMP_RX_VMAX_L : {
		if (max_size >= sizeof(int32_t)) {
			 ret = sktune_api_get_vmax(sktune->handle, data_buf, CHANNEL_LEFT);
			 if (ret < 0) {
				comp_err(dev, "[Awinic] get vmax l failed ");
			 } else {
				buf_size = ret;
			 }
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", max_size);
			ret = -1;
		}
	} break;
	case SOF_SMART_AMP_RX_VMAX_R : {
		if (max_size >= sizeof(int32_t)) {
			 ret = sktune_api_get_vmax(sktune->handle, data_buf, CHANNEL_RIGHT);
			 if (ret < 0) {
				comp_err(dev, "[Awinic] get vmax r failed ");
			 } else {
				buf_size = ret;
			 }
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", max_size);
			ret = -1;
		}
	} break;
	case SOF_SMART_AMP_RX_CALI_CFG_L : {
		 ret = sktune_api_get_start_cali_cfg(sktune->handle, (char *)data_buf, max_size, CHANNEL_LEFT);
		 if (ret < 0) {
			comp_err(dev, "[Awinic] get cali cfg l failed ");
		 } else {
			buf_size = ret;
		 }
	} break;
	case SOF_SMART_AMP_RX_CALI_CFG_R : {
		 ret = sktune_api_get_start_cali_cfg(sktune->handle, (char *)data_buf, max_size, CHANNEL_RIGHT);
		 if (ret < 0) {
			comp_err(dev, "[Awinic] get cali cfg r failed ");
		 } else {
			buf_size = ret;
		 }
	} break;
	case SOF_SMART_AMP_RX_RE_L : {
		if (max_size >= sizeof(int32_t)) {
			 ret = sktune_api_get_cali_re(sktune->handle, data_buf, CHANNEL_LEFT);
			 if (ret < 0) {
				comp_err(dev, "[Awinic] get cali re l failed ");
			 } else {
				buf_size = ret;
			 }
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", max_size);
			ret = -1;
		}
	} break;
	case SOF_SMART_AMP_RX_RE_R : {
		if (max_size >= sizeof(int32_t)) {
			 ret = sktune_api_get_cali_re(sktune->handle, data_buf, CHANNEL_RIGHT);
			 if (ret < 0) {
				comp_err(dev, "[Awinic] get cali re r failed ");
			 } else {
				buf_size = ret;
			 }
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", max_size);
			ret = -1;
		}
	} break;
	case SOF_SMART_AMP_RX_REAL_DATA_L : {
		ret = sktune_api_get_cali_data(sktune->handle, (char *)data_buf, max_size, CHANNEL_LEFT);
		if (ret < 0) {
		   comp_err(dev, "[Awinic] get cali data l failed ");
		} else {
		   buf_size = ret;
		}
	} break; 
	case SOF_SMART_AMP_RX_REAL_DATA_R : {
		ret = sktune_api_get_cali_data(sktune->handle, (char *)data_buf, max_size, CHANNEL_RIGHT);
		if (ret < 0) {
		   comp_err(dev, "[Awinic] get cali data r failed ");
		} else {
		   buf_size = ret;
		}
	} break; 
	case SOF_SMART_AMP_RX_F0_L : {
		if (max_size >= sizeof(int32_t)) {
			 ret = sktune_api_get_cali_f0(sktune->handle, data_buf, CHANNEL_LEFT);
			 if (ret < 0) {
				comp_err(dev, "[Awinic] get f0 l failed ");
			 } else {
				buf_size = ret;
			 }
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", max_size);
			ret = -1;
		}
	} break;
	case SOF_SMART_AMP_RX_F0_R : {
		if (max_size >= sizeof(int32_t)) {
			 ret = sktune_api_get_cali_f0(sktune->handle, data_buf, CHANNEL_RIGHT);
			 if (ret < 0) {
				comp_err(dev, "[Awinic] get f0 r failed ");
			 } else {
				buf_size = ret;
			 }
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", max_size);
			ret = -1;
		}
	} break;
	case SOF_SMART_AMP_RX_SPIN : {
		if (max_size >= sizeof(int32_t)) {
			 ret = sktune_api_get_spin_mode(sktune->handle, (uint32_t *)data_buf);
			 if (ret < 0) {
				comp_err(dev, "[Awinic] get spin failed ");
			 } else {
				buf_size = sizeof(int32_t);
			 }
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", max_size);
			ret = -1;
		}
	} break;
	case SOF_SMART_AMP_RX_MSG : {
		ret = smartpa_amp_get_msg_parser(sktune, dev, (char *)data_buf, max_size, AW_MSG_ID_0);
		if (ret < 0) {
		   comp_err(dev, "[Awinic] get msg failed ");
		} else {
		   buf_size = ret;
		}
	} break;
	default : {
			comp_err(dev, "[Awinic] get unsupport params ID  %d ", params_id);
			ret = -1;
	} break;

	}

	if (ret < 0) {
		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = 0;
		return -1;
	} else {
		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = buf_size;
	}

	return 0;
}


static int smart_amp_set_msg_parser(sktune_t *sktune, struct comp_dev *dev, char *data_buf, uint32_t len, int msg_id)
{
	dsp_msg_hdr_t *msg_hdr = (dsp_msg_hdr_t *)data_buf;
	uint32_t actual_data_len = len;
	char *data_ptr = NULL;
	int ret = 0;

	if (actual_data_len < sizeof(dsp_msg_hdr_t)) {
		comp_err(dev, "[Awinic] msg hdr unmatch ");
		return -1;
	}

	if (msg_hdr->version != DSP_MSG_VERSION) {
		comp_err(dev, "[Awinic] msg hdr version unmatch");
		return -1;
	}

	if (msg_hdr->type == DSP_MSG_TYPE_CMD) {
		sktune->sub_msg_info[msg_id].status = AW_MESG_READY;
		sktune->sub_msg_info[msg_id].opcode_id = msg_hdr->opcode_id;
		comp_info(dev, "[Awinic] set msg opcode[0x%x] done", msg_hdr->opcode_id);
		return 0;
	}

	if (msg_hdr->type == DSP_MSG_TYPE_DATA) {
		actual_data_len = len - sizeof(dsp_msg_hdr_t);
		data_ptr = (char *)(data_buf + sizeof(dsp_msg_hdr_t));
		ret = sktune_api_set_data(sktune->handle, msg_hdr->opcode_id, data_ptr, actual_data_len);
		if (ret < 0) {
			comp_err(dev, "[Awinic] set msg opcode[0x%x] failed", msg_hdr->opcode_id);
			return -1;
		}
	} else {
		comp_err(dev, "[Awinic] unmatch msg type 0x%x", msg_hdr->type);
		return -1;
	}

	comp_info(dev, "[Awinic] set msg opcode[0x%x] done", msg_hdr->opcode_id);
	return 0;
}

int smart_amp_set_param(sktune_t *sktune,
			struct comp_dev *dev,
			struct sof_ipc_ctrl_data *cdata, uint32_t params_id)
{
	int32_t *params_data;
	size_t params_size;
	int ret = 0;
	params_data = (int32_t *)ASSUME_ALIGNED(&cdata->data->data, sizeof(uint32_t));
	params_size = cdata->data->size;

	switch (params_id) {

	case SOF_SMART_AMP_RX_ENABLE : {
		if (params_size >= sizeof(uint32_t)) {
			sktune->enable = *params_data;
			comp_info(dev, "[Awinic] set Enable %d ", sktune->enable);
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", params_size);
			ret = -1;
		}
	} break;
	case SOF_SMART_AMP_RX_SKT_PARAMS : {
		ret = sktune_api_set_params(sktune->handle, (char *)params_data, params_size);
		if (ret < 0) {
			comp_err(dev, "[Awinic] set params failed ");
		}
	} break;

	case SOF_SMART_AMP_RX_VMAX_L : {
		if (params_size >= sizeof(uint32_t)) {
			ret = sktune_api_set_vmax(sktune->handle, *((int32_t*)params_data), CHANNEL_LEFT);
			if (ret < 0) {
				comp_err(dev, "[Awinic] set Vmax L failed ");
			}
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", params_size);
			ret = -1;
		}
	}break;
	case SOF_SMART_AMP_RX_VMAX_R : {
		if (params_size >= sizeof(uint32_t)) {
			ret = sktune_api_set_vmax(sktune->handle, *((int32_t*)params_data), CHANNEL_RIGHT);
			if (ret < 0) {
				comp_err(dev, "[Awinic] set Vmax  R failed ");
			}
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", params_size);
			ret = -1;
		}
	}break;
	case SOF_SMART_AMP_RX_CALI_CFG_L : {
		ret = sktune_api_set_start_cali_cfg(sktune->handle, (char *)params_data, params_size, CHANNEL_LEFT);
		if (ret < 0) {
			comp_err(dev, "[Awinic] set cali cfg failed ");
		}
	}break;
	case SOF_SMART_AMP_RX_CALI_CFG_R : {
		ret = sktune_api_set_start_cali_cfg(sktune->handle, (char *)params_data, params_size, CHANNEL_RIGHT);
		if (ret < 0) {
			comp_err(dev, "[Awinic] set cali cfg failed ");
		}
	}break;

	case SOF_SMART_AMP_RX_RE_L : {
		if (params_size >= sizeof(uint32_t)) {
			ret = sktune_api_set_cali_re(sktune->handle, *((int32_t*)params_data), CHANNEL_LEFT);
			if (ret < 0) {
				comp_err(dev, "[Awinic] set cali Re L failed ");
			}
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", params_size);
			ret = -1;
		}
	}break;

	case SOF_SMART_AMP_RX_RE_R : {
		if (params_size >= sizeof(uint32_t)) {
			ret = sktune_api_set_cali_re(sktune->handle, *((int32_t*)params_data), CHANNEL_RIGHT);
			if (ret < 0) {
				comp_err(dev, "[Awinic] set cali Re L failed ");
			}
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", params_size);
			ret = -1;
		}
	}break;

	case SOF_SMART_AMP_RX_NOISE_L : {
		if (params_size >= sizeof(uint32_t)) {
			ret = sktune_api_set_noise(sktune->handle, *((int32_t*)params_data), CHANNEL_LEFT);
			if (ret < 0) {
				comp_err(dev, "[Awinic] set noise L failed ");
			}
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", params_size);
			ret = -1;
		}
	}break;

	case SOF_SMART_AMP_RX_NOISE_R : {
		if (params_size >= sizeof(uint32_t)) {
			ret = sktune_api_set_noise(sktune->handle, *((int32_t*)params_data), CHANNEL_RIGHT);
			if (ret < 0) {
				comp_err(dev, "[Awinic] set noise R failed ");
			}
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", params_size);
			ret = -1;
		}
	}break;
	case SOF_SMART_AMP_RX_SPIN : {
		if (params_size >= sizeof(uint32_t)) {
			ret = sktune_api_set_spin_mode(sktune->handle, (uint32_t*)params_data);
			if (ret < 0) {
				comp_err(dev, "[Awinic] set spin mode failed ");
			}
		} else {
			comp_err(dev, "[Awinic] bad params size  %d ", params_size);
			ret = -1;
		}
	}break;
	case SOF_SMART_AMP_RX_MSG : {
		ret = smart_amp_set_msg_parser(sktune->handle, dev, (char *)params_data, params_size, AW_MSG_ID_0);
		if (ret < 0) {
			comp_err(dev, "[Awinic] set msg failed");
		}
	} break;
	default : {
			comp_err(dev, "[Awinic] set unsupport params ID  %d ", params_id);
			ret = -1;
	} break;

	}

	comp_info(dev, "[Awinic] params ID  %d set %s", params_id, ret == 0 ? "success" : "failed");
	return ret;
}


static void smart_amp_sktune_free(sktune_t* sktune)
{
	rfree(sktune->frame_in.data_ptr);
	rfree(sktune->frame_out.data_ptr);
	rfree(sktune->frame_iv.data_ptr);
	rfree(sktune->handle);
	rfree(sktune);
}

sktune_t* smart_amp_sktune_alloc(struct comp_dev *dev)
{
	uint32_t sum_mem_size = 0;
	unsigned int algo_size;
	uint32_t size;
	sktune_t* sktune = NULL;


	/*memory allocation for sktune*/
	sktune = rballoc(0, SOF_MEM_CAPS_RAM, sizeof(sktune_t));
	if (!sktune) {
		comp_err(dev, "[Awinic] SKTune alloc failed!");
		return NULL;
	}
	memset(sktune, 0, sizeof(sktune_t));


	/*buffer:sof -> frame_in memory allocation*/
	size = SMART_AMP_FF_BUF_DB_SZ * sizeof(int32_t);
	sktune->frame_in.data_ptr =  rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!sktune->frame_in.data_ptr) {
		comp_err(dev, "[Awinic] frame_in alloc failed");
		sktune->frame_in.actual_data_len = 0;
		sktune->frame_in.max_data_len = 0;
		goto err;
	}
	sktune->frame_in.actual_data_len = 0;
	sktune->frame_in.max_data_len = size;
	sum_mem_size += size;


	/*buffer:sof -> frame_out memory allocation*/
	size = SMART_AMP_FF_BUF_DB_SZ * sizeof(int32_t);
	sktune->frame_out.data_ptr =  rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!sktune->frame_out.data_ptr) {
		comp_err(dev, "[Awinic] frame_out alloc failed");
		sktune->frame_out.actual_data_len = 0;
		sktune->frame_out.max_data_len = 0;
		goto err;
	}
	sktune->frame_out.actual_data_len = 0;
	sktune->frame_out.max_data_len = size;
	sum_mem_size += size;

	/* buffer : sof -> frame_iv memory allocation */
	size = SMART_AMP_FB_BUF_DB_SZ * sizeof(int32_t);
	sktune->frame_iv.data_ptr = rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!sktune->frame_iv.data_ptr) {
		comp_err(dev, "[Awinic] frame_iv alloc failed");
		sktune->frame_iv.actual_data_len = 0;
		sktune->frame_iv.max_data_len = 0;
		goto err;
	}
	sktune->frame_iv.actual_data_len = 0;
	sktune->frame_iv.max_data_len = size;

	sum_mem_size += size;


	/* memory allocation of SKTune handle */
	algo_size = sktune_api_get_size();
	if(algo_size == 0) {
		comp_err(dev, "[Awinic] get memory failed, algo_size =  %d", algo_size);
		goto err;
	}

	sktune->handle = rballoc(0, SOF_MEM_CAPS_RAM, algo_size);
	if (!sktune->handle) {
		comp_err(dev, "[Awinic] SKTune handle alloc failed");
		goto err;
	}

	sum_mem_size += (int)algo_size;

	comp_dbg(dev, "[Awinic] module:%p (%d bytes used)", sktune, sum_mem_size);

	return sktune;

err:
	smart_amp_sktune_free(sktune);
	rfree(sktune);
	return NULL;

}


int smart_amp_init(sktune_t *      sktune, struct comp_dev *dev)
{
	int ret;

	ret = sktune_api_init(sktune->handle);
	if (ret < 0) {
		comp_err(dev, "[Awinic] SKTune init failed");
		return ret;
	}

	sktune_api_set_media_info(sktune->handle, (void *)&sktune->media_info);

	sktune->sub_msg_info[AW_MSG_ID_0].status = AW_MESG_NONE;
	sktune->sub_msg_info[AW_MSG_ID_1].status = AW_MESG_NONE;
	sktune->sub_msg_info[AW_MSG_ID_0].opcode_id = 0;
	sktune->sub_msg_info[AW_MSG_ID_1].opcode_id = 0;

	return 0;
}

int smart_amp_deinit(sktune_t *       sktune, struct comp_dev *dev)
{
	int ret;

	ret = sktune_api_end(sktune->handle);
	if (ret < 0) {
		comp_err(dev, "[Awinic] Awinic End  failed");
	}

	smart_amp_sktune_free(sktune);

	return 0;
}


int smart_amp_check_audio_fmt(int sample_rate, int ch_num)
{

	if (sample_rate != 4800) {
		return -1;
	}

	if (ch_num != 2) {
		return -1;
	}

	return 0;
}

int smart_amp_flush(sktune_t *sktune, struct comp_dev *dev)
{
	memset(sktune->frame_in.data_ptr, 0,
	       SMART_AMP_FF_BUF_DB_SZ * sizeof(int32_t));
	memset(sktune->frame_out.data_ptr, 0,
	       SMART_AMP_FF_BUF_DB_SZ * sizeof(int32_t));
	memset(sktune->frame_iv.data_ptr, 0,
	       SMART_AMP_FB_BUF_DB_SZ * sizeof(int32_t));


	comp_dbg(dev, "[Awinic] Reset (handle:%p)", sktune);

	return 0;
}


static int smart_amp_get_buffer(char *buf, uint32_t frames,
				const struct audio_stream __sparse_cache *stream, uint32_t num_ch)
{
	int idx;
	uint32_t sample_num = frames * num_ch;
	int byte_num = 0;
	int16_t *input16, *output16;
	int32_t *input32, *output32;

	input16 = audio_stream_get_rptr(stream);
	input32 = audio_stream_get_rptr(stream);
	output16 = (int16_t *)buf;
	output32 = (int32_t *)buf;

	switch (audio_stream_get_frm_fmt(stream)) {
	case SOF_IPC_FRAME_S16_LE:
		for (idx = 0 ; idx < sample_num ; idx++) {
				input16 = audio_stream_read_frag_s16(stream, idx);
				output16[idx] = *input16;
		}
		byte_num =  sample_num * sizeof(int16_t);
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		for (idx = 0 ; idx < sample_num ; idx++) {
				input32 = audio_stream_read_frag_s32(stream, idx);
				output32[idx] = *input32;
		}
		byte_num =  sample_num * sizeof(int32_t);
		break;
	default:
		return -1;
	}
	return byte_num;
}


int smart_amp_fb_data_prepare(sktune_t *       sktune, struct comp_dev *dev,
				const struct audio_stream __sparse_cache *source,
			     uint32_t frames)

{
	int num_ch, ret;

	if (frames == 0) {
		comp_warn(dev, "[Awinic] feedback frame size zero warning.");
		return 0;
	}

	if (frames > SMART_AMP_FB_BUF_DB_SZ) {
		comp_err(dev, "[Awinic] feedback frame size overflow  : %d", frames);
		return -1;
	}

	num_ch = MIN(audio_stream_get_channels(source), SMART_AMP_FB_MAX_CH_NUM);

	ret = smart_amp_get_buffer(sktune->frame_iv.data_ptr, frames, source, num_ch);
	if (ret < 0) {
		comp_err(dev, "[Awinic] get IV buf failed");
		sktune->frame_iv.actual_data_len = 0;
		return -1;
	}
	
	sktune->frame_iv.actual_data_len = (uint32_t)ret;


	return 0;
}


int smart_amp_ff_data_prepare(sktune_t *       sktune, struct comp_dev *dev,
				const struct audio_stream __sparse_cache *source,
			     uint32_t frames)
{
	int num_ch, ret;
	
	if (frames == 0) {
		comp_warn(dev, "[Awinic] ff frame size zero warning.");
		return 0;
	}

	if (frames > SMART_AMP_FF_BUF_DB_SZ) {
		comp_err(dev, "[Awinic] ff frame size overflow  : %d", frames);
		return -1;
	}

	num_ch = MIN(audio_stream_get_channels(source), SMART_AMP_FB_MAX_CH_NUM);

	ret = smart_amp_get_buffer(sktune->frame_in.data_ptr, frames, source, num_ch);
	if (ret < 0) {
		sktune->frame_in.actual_data_len = 0;
		comp_err(dev, "[Awinic] get rx buf failed");
		return -1;
	}

	sktune->frame_in.actual_data_len = (uint32_t)ret;

	return 0;
}


static int smart_amp_put_buffer(char *buf, uint32_t frames,
				const struct audio_stream __sparse_cache *stream, 
				uint32_t num_ch_out)
{
	uint32_t sample_num = num_ch_out * frames;
	int idx, ch;
	int16_t *input16, *output16;
	int32_t *input32, *output32;

	input16 = (int16_t *)buf;
	input32 = (int32_t *)buf;
	output16 = audio_stream_get_wptr(stream);
	output32 = audio_stream_get_wptr(stream);

	switch (audio_stream_get_frm_fmt(stream)) {
	case SOF_IPC_FRAME_S16_LE:
		for (idx = 0 ; idx < sample_num ; idx++) {
				output16 = audio_stream_write_frag_s16(stream, idx);
				*output16 = input16[idx];
		}
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		for (idx = 0 ; idx < sample_num ; idx++) {
			for (ch = 0 ; ch < num_ch_out; ch++) {
				output32 = audio_stream_write_frag_s32(stream, idx);
				*output32 = input32[idx];
			}
		}
		break;
	default:
		return -1;
	}
	return 0;
}


int smart_amp_process(sktune_t *       sktune, struct comp_dev *dev,
				const struct audio_stream __sparse_cache *source,
				const struct audio_stream __sparse_cache *sink,
			     uint32_t frames, int num_ch_out)
{
	int ret;

	ret = sktune_api_process(sktune->handle, &sktune->frame_in, &sktune->frame_iv);
	if (ret < 0) {
		comp_err(dev, "[Awinic] sktune process error");
		return -1;
	}


	ret = smart_amp_put_buffer(sktune->frame_in.data_ptr,
			   frames, sink,
			   MIN(num_ch_out, SMART_AMP_FF_OUT_MAX_CH_NUM));

	return 0;
}


