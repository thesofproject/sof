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

	initparam.isamplebitwidth       = hspk->bitwidth;
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

static int maxim_dsm_get_all_param(struct smart_amp_mod_struct_t *hspk,
				   struct comp_dev *dev)
{
	struct smart_amp_caldata *caldata = &hspk->param.caldata;
	int32_t *db = (int32_t *)caldata->data;
	enum DSM_API_MESSAGE retcode;
	int cmdblock[DSM_GET_PARAM_SZ_PAYLOAD];
	int num_param = hspk->param.max_param;
	int idx;

	for (idx = 0 ; idx < num_param ;  idx++) {
		/* Read all DSM parameters - Please refer to the API header file
		 * for more details about get_params() usage info.
		 */
		cmdblock[DSM_GET_ID_IDX] = DSM_SET_CMD_ID(idx);
		retcode = dsm_api_get_params(hspk->dsmhandle, 1, (void *)cmdblock);
		if (retcode != DSM_API_OK) {
			/* set zero if the parameter is not readable */
			cmdblock[DSM_GET_CH1_IDX] = 0;
			cmdblock[DSM_GET_CH2_IDX] = 0;
		}

		/* fill the data for the 1st channel 4 byte ID + 4 byte value */
		db[idx * DSM_PARAM_MAX + DSM_PARAM_ID] = DSM_CH1_BITMASK | idx;
		db[idx * DSM_PARAM_MAX + DSM_PARAM_VALUE] = cmdblock[DSM_GET_CH1_IDX];
		/* fill the data for the 2nd channel 4 byte ID + 4 byte value
		 * 2nd channel data have offset for num_param * DSM_PARAM_MAX
		 */
		db[(idx + num_param) * DSM_PARAM_MAX + DSM_PARAM_ID] = DSM_CH2_BITMASK | idx;
		db[(idx + num_param) * DSM_PARAM_MAX + DSM_PARAM_VALUE] = cmdblock[DSM_GET_CH2_IDX];
	}

	return 0;
}

static int maxim_dsm_get_volatile_param(struct smart_amp_mod_struct_t *hspk,
					struct comp_dev *dev)
{
	struct smart_amp_caldata *caldata = &hspk->param.caldata;
	int32_t *db = (int32_t *)caldata->data;
	enum DSM_API_MESSAGE retcode;
	int cmdblock[DSM_GET_PARAM_SZ_PAYLOAD];
	int num_param = hspk->param.max_param;
	int idx;

	/* Update all volatile parameter values */
	for (idx = DSM_API_ADAPTIVE_PARAM_START ; idx <= DSM_API_ADAPTIVE_PARAM_END ;  idx++) {
		cmdblock[0] = DSM_SET_CMD_ID(idx);
		retcode = dsm_api_get_params(hspk->dsmhandle, 1, (void *)cmdblock);
		if (retcode != DSM_API_OK)
			return -EINVAL;

		/* fill the data for the 1st channel 4 byte ID + 4 byte value */
		db[idx * DSM_PARAM_MAX + DSM_PARAM_ID] = DSM_CH1_BITMASK | idx;
		db[idx * DSM_PARAM_MAX + DSM_PARAM_VALUE] = cmdblock[DSM_GET_CH1_IDX];
		/* fill the data for the 2nd channel 4 byte ID + 4 byte value
		 * 2nd channel data have offset for num_param * DSM_PARAM_MAX
		 */
		db[(idx + num_param) * DSM_PARAM_MAX + DSM_PARAM_ID] = DSM_CH2_BITMASK | idx;
		db[(idx + num_param) * DSM_PARAM_MAX + DSM_PARAM_VALUE] = cmdblock[DSM_GET_CH2_IDX];
	}

	return 0;
}

int maxim_dsm_get_param_forced(struct smart_amp_mod_struct_t *hspk,
			       struct comp_dev *dev)
{
	struct smart_amp_caldata *caldata = &hspk->param.caldata;
	int32_t *db = (int32_t *)caldata->data;
	enum DSM_API_MESSAGE retcode;
	int cmdblock[DSM_GET_PARAM_SZ_PAYLOAD];
	int num_param = hspk->param.max_param;
	int idx;

	/* Update all parameter values from the DSM component */
	for (idx = 0 ; idx <= num_param ;  idx++) {
		cmdblock[0] = DSM_SET_CMD_ID(idx);
		retcode = dsm_api_get_params(hspk->dsmhandle, 1, (void *)cmdblock);
		if (retcode != DSM_API_OK) {
			/* set zero if the parameter is not readable */
			cmdblock[DSM_GET_CH1_IDX] = 0;
			cmdblock[DSM_GET_CH2_IDX] = 0;
		}
		/* fill the data for the 1st channel 4 byte ID + 4 byte value */
		db[idx * DSM_PARAM_MAX + DSM_PARAM_ID] = DSM_CH1_BITMASK | idx;
		db[idx * DSM_PARAM_MAX + DSM_PARAM_VALUE] = cmdblock[DSM_GET_CH1_IDX];
		/* fill the data for the 2nd channel 4 byte ID + 4 byte value
		 * 2nd channel data have offset for num_param * DSM_PARAM_MAX
		 */
		db[(idx + num_param) * DSM_PARAM_MAX + DSM_PARAM_ID] = DSM_CH2_BITMASK | idx;
		db[(idx + num_param) * DSM_PARAM_MAX + DSM_PARAM_VALUE] = cmdblock[DSM_GET_CH2_IDX];
	}

	return 0;
}

int maxim_dsm_get_param(struct smart_amp_mod_struct_t *hspk,
			struct comp_dev *dev,
			struct sof_ipc_ctrl_data *cdata, int size)
{
	struct smart_amp_caldata *caldata = &hspk->param.caldata;
	size_t bs;
	int ret = 0;

	if (caldata->data) {
		/* reset data_pos variable in case of copying first element */
		if (!cdata->msg_index) {
			caldata->data_pos = 0;
			/* update volatile parameters */
			ret = maxim_dsm_get_volatile_param(hspk, dev);
			if (ret)
				return -EINVAL;
		}

		bs = cdata->num_elems;

		/* return an error in case of mismatch between num_elems and
		 * required size
		 */
		if (bs > size) {
			comp_err(dev, "[DSM] maxim_dsm_get_param(): invalid size %d", bs);
			return -EINVAL;
		}

		/* copy required size of data */
		ret = memcpy_s(cdata->data->data, size,
			       (char *)caldata->data + caldata->data_pos,
			       bs);

		assert(!ret);

		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = caldata->data_size;
		caldata->data_pos += bs;
	} else {
		comp_warn(dev, "[DSM] caldata->data not allocated yet.");
		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = 0;
	}
	return 0;
}

int maxim_dsm_set_param(struct smart_amp_mod_struct_t *hspk,
			struct comp_dev *dev,
			struct sof_ipc_ctrl_data *cdata)
{
	struct smart_amp_param_struct_t *param = &hspk->param;
	struct smart_amp_caldata *caldata = &hspk->param.caldata;
	/* Model database */
	int32_t *db = (int32_t *)caldata->data;
	/* Payload buffer */
	int *wparam = (int *)cdata->data->data;
	/* number of parameters to read */
	int num_param = (cdata->num_elems >> 2);
	int idx, id, ch;

	if (!cdata->msg_index) {
		/* reset variables for the first set_param frame is arrived */
		param->pos = 0;		/* number of received parameters */
		param->param.id = 0;	/* variable to keep last parameter ID */
		param->param.value = 0;	/* variable to keep last parameter value */
	}

	for (idx = 0 ; idx < num_param ; idx++) {
		/* Single DSM parameter consists of ID and value field (total 8 bytes)
		 * It is even number aligned, but actual payload could be odd number.
		 * Actual setparam operation is performed when both ID and value are
		 * ready
		 */
		if (param->pos % 2 == 0) {
			/* even field is ID */
			param->param.id = wparam[idx];
		} else {
			/* odd field is value */
			int value[DSM_SET_PARAM_SZ_PAYLOAD];
			enum DSM_API_MESSAGE retcode;

			param->param.value = wparam[idx];
			value[DSM_SET_ID_IDX] = param->param.id;
			value[DSM_SET_VALUE_IDX] = param->param.value;

			id = DSM_CH_MASK(param->param.id);
			ch = param->param.id & DSM_CH1_BITMASK ? 0 : 1;

			/* 2nd channel has (hspk->param.max_param * DSM_PARAM_MAX) sized offset */
			db[(id + ch * hspk->param.max_param) * DSM_PARAM_MAX + DSM_PARAM_VALUE] =
				param->param.value;

			/* More detailed information about set_params() function is available
			 * in the api header file
			 */
			retcode = dsm_api_set_params(hspk->dsmhandle, 1, value);
			if (retcode != DSM_API_OK) {
				comp_err(dev, "[DSM] maxim_dsm_set_param() write failure. (id:%x, ret:%x)",
					 id, retcode);
				return -EINVAL;
			}
		}
		param->pos++;
	}

	return 0;
}

int maxim_dsm_restore_param(struct smart_amp_mod_struct_t *hspk,
			    struct comp_dev *dev)
{
	struct smart_amp_caldata *caldata = &hspk->param.caldata;
	int32_t *db = (int32_t *)caldata->data;
	int num_param = hspk->param.max_param;
	int value[DSM_SET_PARAM_SZ_PAYLOAD];
	enum DSM_API_MESSAGE retcode;
	int idx;

	/* Restore parameter values in db to the DSM component */
	for (idx = 0 ; (idx < num_param << 1) ; idx++) {
		value[DSM_SET_ID_IDX] = db[idx * DSM_PARAM_MAX + DSM_PARAM_ID];
		value[DSM_SET_VALUE_IDX] = db[idx * DSM_PARAM_MAX + DSM_PARAM_VALUE];

		retcode = dsm_api_set_params(hspk->dsmhandle, 1, value);
		if (retcode != DSM_API_OK) {
			comp_err(dev, "[DSM] maxim_dsm_restore_param() write failure. (id:%x, ret:%x)",
				 value[DSM_SET_ID_IDX], retcode);
			return -EINVAL;
		}
	}
	return 0;
}

static void maxim_dsm_ff_proc(struct smart_amp_mod_struct_t *hspk,
			      struct comp_dev *dev, void *in, void *out,
			      int nsamples, int szsample)
{
	union smart_amp_buf buf, buf_out;
	int16_t *input = (int16_t *)hspk->buf.input;
	int16_t *output = (int16_t *)hspk->buf.output;
	int32_t *input32 = hspk->buf.input;
	int32_t *output32 = hspk->buf.output;
	int *w_ptr = &hspk->buf.ff.avail;
	int *r_ptr = &hspk->buf.ff_out.avail;
	bool is_16bit = szsample == 2 ? true : false;
	int remain;
	int idx;

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
			for (idx = 0; idx < DSM_FRM_SZ; idx++) {
				input[idx] = (buf.buf16[2 * idx]);
				input[idx + DSM_FRM_SZ] =
					(buf.buf16[2 * idx + 1]);
			}
		} else {
			for (idx = 0; idx < DSM_FRM_SZ; idx++) {
				input32[idx] = buf.buf32[2 * idx];
				input32[idx + DSM_FRM_SZ] =
					buf.buf32[2 * idx + 1];
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
			for (idx = 0; idx < DSM_FRM_SZ; idx++) {
				/* Buffer re-ordering LR/LR/LR */
				buf_out.buf16[*r_ptr + 2 * idx] = (output[idx]);
				buf_out.buf16[*r_ptr + 2 * idx + 1] =
					output[idx + DSM_FRM_SZ];
			}
		} else {
			dsm_api_ff_process(hspk->dsmhandle, hspk->channelmask,
					   (short *)input32, &hspk->ifsamples,
					   (short *)output32, &hspk->ofsamples);
			for (idx = 0; idx < DSM_FRM_SZ; idx++) {
				buf_out.buf32[*r_ptr + 2 * idx] = output32[idx];
				buf_out.buf32[*r_ptr + 2 * idx + 1] =
					output32[idx + DSM_FRM_SZ];
			}
		}

		*r_ptr += DSM_FF_BUF_SZ;
	}

	/* Output buffer preparation */
	if (*r_ptr >= nsamples) {
		if (is_16bit)
			memcpy_s(out, nsamples * szsample,
				 buf_out.buf16, nsamples * szsample);
		else
			memcpy_s(out, nsamples * szsample,
				 buf_out.buf32, nsamples * szsample);

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
	int16_t *v = (int16_t *)hspk->buf.voltage;
	int16_t *i = (int16_t *)hspk->buf.current;
	int32_t *v32 = hspk->buf.voltage;
	int32_t *i32 = hspk->buf.current;
	bool is_16bit = szsample == 2 ? true : false;
	int remain;
	int idx;

	buf.buf16 = (int16_t *)hspk->buf.fb.buf;
	buf.buf32 = hspk->buf.fb.buf;

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
			for (idx = 0; idx < DSM_FRM_SZ; idx++) {
			/* Buffer ordering for DSM : VIVI... -> VV... II...*/
				v[idx] = buf.buf16[4 * idx];
				i[idx] = buf.buf16[4 * idx + 1];
				v[idx + DSM_FRM_SZ] = buf.buf16[4 * idx + 2];
				i[idx + DSM_FRM_SZ] = buf.buf16[4 * idx + 3];
			}
		} else {
			for (idx = 0; idx < DSM_FRM_SZ; idx++) {
				v[idx] = buf.buf32[4 * idx];
				i[idx] = buf.buf32[4 * idx + 1];
				v[idx + DSM_FRM_SZ] =
					buf.buf32[4 * idx + 2];
				i[idx + DSM_FRM_SZ] =
					buf.buf32[4 * idx + 3];
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
		if (is_16bit)
			dsm_api_fb_process(hspk->dsmhandle,
					   hspk->channelmask,
					   i, v, &hspk->ibsamples);
		else
			dsm_api_fb_process(hspk->dsmhandle,
					   hspk->channelmask,
					   (short *)i32, (short *)v32, &hspk->ibsamples);
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

	comp_dbg(dev, "[DSM] Reset (handle:%p)", hspk);

	return 0;
}

int smart_amp_init(struct smart_amp_mod_struct_t *hspk, struct comp_dev *dev)
{
	return maxim_dsm_init(hspk, dev);
}

int smart_amp_get_all_param(struct smart_amp_mod_struct_t *hspk,
			    struct comp_dev *dev)
{
	if (maxim_dsm_get_all_param(hspk, dev) < 0)
		return -EINVAL;
	return 0;
}

int smart_amp_get_num_param(struct smart_amp_mod_struct_t *hspk,
			    struct comp_dev *dev)
{
	enum DSM_API_MESSAGE retcode;
	int cmdblock[DSM_GET_PARAM_SZ_PAYLOAD];

	/* Get number of parameters */
	cmdblock[DSM_GET_ID_IDX] = DSM_SET_CMD_ID(DSM_API_GET_MAXIMUM_CMD_ID);
	retcode = dsm_api_get_params(hspk->dsmhandle, 1, (void *)cmdblock);
	if (retcode != DSM_API_OK)
		return 0;

	return MIN(DSM_DEFAULT_MAX_NUM_PARAM, cmdblock[DSM_GET_CH1_IDX]);
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
	int idx, ch;
	uint32_t in_frag = 0;
	union smart_amp_buf input, output;
	int index;

	input.buf16 = (int16_t *)stream->r_ptr;
	input.buf32 = (int32_t *)stream->r_ptr;
	output.buf16 = (int16_t *)buf;
	output.buf32 = (int32_t *)buf;

	switch (stream->frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		for (idx = 0 ; idx < frames ; idx++) {
			for (ch = 0 ; ch < num_ch; ch++) {
				if (chan_map[ch] == -1)
					continue;
				index = in_frag + chan_map[ch];
				input.buf16 =
					audio_stream_read_frag_s16(stream,
								   index);
				output.buf16[num_ch * idx + ch] = *input.buf16;
			}
			in_frag += stream->channels;
		}
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		for (idx = 0 ; idx < frames ; idx++) {
			for (ch = 0 ; ch < num_ch ; ch++) {
				if (chan_map[ch] == -1)
					continue;
				index = in_frag + chan_map[ch];
				input.buf32 =
					audio_stream_read_frag_s32(stream,
								   index);
				output.buf32[num_ch * idx + ch] = *input.buf32;
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
	int idx, ch;

	input.buf16 = (int16_t *)buf;
	input.buf32 = (int32_t *)buf;
	output.buf16 = (int16_t *)stream->w_ptr;
	output.buf32 = (int32_t *)stream->w_ptr;

	switch (stream->frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		for (idx = 0 ; idx < frames ; idx++) {
			for (ch = 0 ; ch < num_ch_out; ch++) {
				if (chan_map[ch] == -1) {
					out_frag++;
					continue;
				}
				output.buf16 =
					audio_stream_write_frag_s16(stream,
								    out_frag);
				*output.buf16 = input.buf16[num_ch_in * idx + ch];
				out_frag++;
			}
		}
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		for (idx = 0 ; idx < frames ; idx++) {
			for (ch = 0 ; ch < num_ch_out; ch++) {
				if (chan_map[ch] == -1) {
					out_frag++;
					continue;
				}
				output.buf32 =
					audio_stream_write_frag_s32(stream,
								    out_frag);
				*output.buf32 = input.buf32[num_ch_in * idx + ch];
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
		comp_warn(dev, "[DSM] feed forward frame size zero warning.");
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
		comp_warn(dev, "[DSM] feedback frame size zero warning.");
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
