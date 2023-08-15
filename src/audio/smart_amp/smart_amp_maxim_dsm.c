// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Maxim Integrated All rights reserved.
//
// Author: Ryan Lee <ryans.lee@maximintegrated.com>

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
#include <sof/audio/smart_amp/smart_amp.h>
#include "dsm_api_public.h"

/* Maxim DSM(Dynamic Speaker Management) process buffer size */
#define DSM_FRM_SZ		48
#define DSM_FF_BUF_SZ		(DSM_FRM_SZ * SMART_AMP_FF_MAX_CH_NUM)
#define DSM_FB_BUF_SZ		(DSM_FRM_SZ * SMART_AMP_FB_MAX_CH_NUM)

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

static const int supported_fmt_count = 3;
static const uint16_t supported_fmts[] = {
	SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE
};

union maxim_dsm_buf {
	int16_t *buf16;
	int32_t *buf32;
};

struct maxim_dsm_ff_buf_struct_t {
	int32_t *buf;
	int avail;
};

struct maxim_dsm_fb_buf_struct_t {
	int32_t *buf;
	int avail;
	int rdy;
};

struct smart_amp_buf_struct_t {
	/* buffer : feed forward process input */
	int32_t *input;
	/* buffer : feed forward process output */
	int32_t *output;
	/* buffer : feedback voltage */
	int32_t *voltage;
	/* buffer : feedback current */
	int32_t *current;
	/* buffer : feed forward variable length -> fixed length */
	struct maxim_dsm_ff_buf_struct_t ff;
	/* buffer : feed forward variable length <- fixed length */
	struct maxim_dsm_ff_buf_struct_t ff_out;
	/* buffer : feedback variable length -> fixed length */
	struct maxim_dsm_fb_buf_struct_t fb;
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

/* self-declared inner model data struct */
struct smart_amp_mod_struct_t {
	struct smart_amp_mod_data_base base;
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

static int maxim_dsm_init(struct smart_amp_mod_struct_t *hspk)
{
	const struct comp_dev *dev = hspk->base.dev;
	int *circularbuffersize = hspk->circularbuffersize;
	int *delayedsamples = hspk->delayedsamples;
	struct dsm_api_init_ext_t initparam;
	enum DSM_API_MESSAGE retcode;

	initparam.isamplebitwidth       = hspk->bitwidth;
	initparam.ichannels             = DSM_DEFAULT_NUM_CHANNEL;
	initparam.ipcircbuffersizebytes = circularbuffersize;
	initparam.ipdelayedsamples      = delayedsamples;
	initparam.isamplingrate         = DSM_DEFAULT_SAMPLE_RATE;

	if (!hspk->dsmhandle) {
		comp_err(dev, "[DSM] Initialization failed: dsmhandle not allocated");
		return -EINVAL;
	}

	retcode = dsm_api_init(hspk->dsmhandle, &initparam,
			       sizeof(struct dsm_api_init_ext_t));
	if (retcode != DSM_API_OK) {
		goto exit;
	} else {
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

static int maxim_dsm_get_num_param(struct smart_amp_mod_struct_t *hspk)
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

static int maxim_dsm_get_handle_size(struct smart_amp_mod_struct_t *hspk)
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

static int maxim_dsm_flush(struct smart_amp_mod_struct_t *hspk)
{
	const struct comp_dev *dev = hspk->base.dev;

	memset(hspk->buf.input, 0, DSM_FF_BUF_SZ * sizeof(int32_t));
	memset(hspk->buf.output, 0, DSM_FF_BUF_SZ * sizeof(int32_t));
	memset(hspk->buf.voltage, 0, DSM_FF_BUF_SZ * sizeof(int32_t));
	memset(hspk->buf.current, 0, DSM_FF_BUF_SZ * sizeof(int32_t));

	memset(hspk->buf.ff.buf, 0, DSM_FF_BUF_DB_SZ * sizeof(int32_t));
	memset(hspk->buf.ff_out.buf, 0, DSM_FF_BUF_DB_SZ * sizeof(int32_t));
	memset(hspk->buf.fb.buf, 0, DSM_FB_BUF_DB_SZ * sizeof(int32_t));

	hspk->buf.ff.avail = DSM_FF_BUF_SZ;
	hspk->buf.ff_out.avail = 0;
	hspk->buf.fb.avail = 0;

	comp_dbg(dev, "[DSM] Reset (handle:%p)", hspk);

	return 0;
}

static int maxim_dsm_get_all_param(struct smart_amp_mod_struct_t *hspk)
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
					const struct comp_dev *dev)
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

static int maxim_dsm_get_param(struct smart_amp_mod_struct_t *hspk,
			       struct sof_ipc_ctrl_data *cdata, int size)
{
	const struct comp_dev *dev = hspk->base.dev;
	struct smart_amp_caldata *caldata = &hspk->param.caldata;
	size_t bs;
	int ret;

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

static int maxim_dsm_set_param(struct smart_amp_mod_struct_t *hspk,
			       struct sof_ipc_ctrl_data *cdata)
{
	const struct comp_dev *dev = hspk->base.dev;
	struct smart_amp_param_struct_t *param = &hspk->param;
	struct smart_amp_caldata *caldata = &hspk->param.caldata;
	/* Model database */
	int32_t *db = (int32_t *)caldata->data;
	/* Payload buffer */
	uint32_t *wparam = (uint32_t *)ASSUME_ALIGNED(&cdata->data->data, 4);
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
			ch = (param->param.id & DSM_CH1_BITMASK) ? 0 : 1;

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

static int maxim_dsm_restore_param(struct smart_amp_mod_struct_t *hspk)
{
	const struct comp_dev *dev = hspk->base.dev;
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

/**
 * mod_ops implementation.
 */

static int maxim_dsm_get_config(struct smart_amp_mod_data_base *mod,
				struct sof_ipc_ctrl_data *cdata, uint32_t size)
{
	struct smart_amp_mod_struct_t *hspk = (struct smart_amp_mod_struct_t *)mod;

	return maxim_dsm_get_param(hspk, cdata, size);
}

static int maxim_dsm_set_config(struct smart_amp_mod_data_base *mod,
				struct sof_ipc_ctrl_data *cdata)
{
	struct smart_amp_mod_struct_t *hspk = (struct smart_amp_mod_struct_t *)mod;

	return maxim_dsm_set_param(hspk, cdata);
}

static int maxim_dsm_ff_proc(struct smart_amp_mod_data_base *mod,
			     uint32_t frames,
			     struct smart_amp_mod_stream *in,
			     struct smart_amp_mod_stream *out)
{
	struct smart_amp_mod_struct_t *hspk = (struct smart_amp_mod_struct_t *)mod;
	union maxim_dsm_buf buf, buf_out;
	int16_t *input = (int16_t *)hspk->buf.input;
	int16_t *output = (int16_t *)hspk->buf.output;
	int32_t *input32 = hspk->buf.input;
	int32_t *output32 = hspk->buf.output;
	int *w_ptr = &hspk->buf.ff.avail;
	int *r_ptr = &hspk->buf.ff_out.avail;
	bool is_16bit = (in->frame_fmt == SOF_IPC_FRAME_S16_LE);
	int szsample = (is_16bit ? 2 : 4);
	int nsamples = frames * in->channels;
	int remain;
	int idx;
	int ret = 0;

	buf.buf16 = (int16_t *)hspk->buf.ff.buf;
	buf.buf32 = (int32_t *)hspk->buf.ff.buf;
	buf_out.buf16 = (int16_t *)hspk->buf.ff_out.buf;
	buf_out.buf32 = (int32_t *)hspk->buf.ff_out.buf;

	/* Report all frames consumed even if buffer overflow to prevent source
	 * congestion. Same for frames produced to keep the stream rolling.
	 */
	in->consumed = frames;
	out->produced = frames;

	/* Current pointer(w_ptr) + number of input frames(nsamples)
	 * must be smaller than buffer size limit
	 */
	if (*w_ptr + nsamples <= DSM_FF_BUF_DB_SZ) {
		if (is_16bit)
			memcpy_s(&buf.buf16[*w_ptr], nsamples * szsample,
				 in->buf.data, nsamples * szsample);
		else
			memcpy_s(&buf.buf32[*w_ptr], nsamples * szsample,
				 in->buf.data, nsamples * szsample);
		*w_ptr += nsamples;
	} else {
		comp_warn(mod->dev,
			  "[DSM] Feed Forward buffer overflow. (w_ptr : %d + %d > %d)",
			  *w_ptr, nsamples, DSM_FF_BUF_DB_SZ);
		ret = -EOVERFLOW;
		goto error;
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
			memcpy_s(out->buf.data, nsamples * szsample,
				 buf_out.buf16, nsamples * szsample);
		else
			memcpy_s(out->buf.data, nsamples * szsample,
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
		return ret;
	}
	/* else { */
	comp_err(mod->dev, "[DSM] DSM FF process underrun. r_ptr : %d", *r_ptr);
	ret = -ENODATA;

error:
	/* TODO(Maxim): undefined behavior when buffer overflow in previous code.
	 *              It leads to early return and no sample written to output
	 *              buffer. However the sink buffer will still writeback
	 *              avail_frames data copied from output buffer.
	 */
	/* set all-zero output when buffer overflow or process underrun. */
	memset_s(out->buf.data, out->buf.size, 0, nsamples * szsample);
	return ret;
}

static int maxim_dsm_fb_proc(struct smart_amp_mod_data_base *mod,
			     uint32_t frames,
			     struct smart_amp_mod_stream *in)
{
	struct smart_amp_mod_struct_t *hspk = (struct smart_amp_mod_struct_t *)mod;
	union maxim_dsm_buf buf;
	int *w_ptr = &hspk->buf.fb.avail;
	int16_t *v = (int16_t *)hspk->buf.voltage;
	int16_t *i = (int16_t *)hspk->buf.current;
	int32_t *v32 = hspk->buf.voltage;
	int32_t *i32 = hspk->buf.current;
	bool is_16bit = (in->frame_fmt == SOF_IPC_FRAME_S16_LE);
	int szsample = (is_16bit ? 2 : 4);
	int nsamples = frames * in->channels;
	int remain;
	int idx;

	buf.buf16 = (int16_t *)hspk->buf.fb.buf;
	buf.buf32 = hspk->buf.fb.buf;

	/* Set all frames consumed even if buffer overflow to prevent source
	 * congestion.
	 */
	in->consumed = frames;

	/* Current pointer(w_ptr) + number of input frames(nsamples)
	 * must be smaller than buffer size limit
	 */
	if (*w_ptr + nsamples <= DSM_FB_BUF_DB_SZ) {
		if (is_16bit)
			memcpy_s(&buf.buf16[*w_ptr], nsamples * szsample,
				 in->buf.data, nsamples * szsample);
		else
			memcpy_s(&buf.buf32[*w_ptr], nsamples * szsample,
				 in->buf.data, nsamples * szsample);
		*w_ptr += nsamples;
	} else {
		comp_warn(mod->dev, "[DSM] Feedback buffer overflow. w_ptr : %d",
			  *w_ptr);
		return -EOVERFLOW;
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
	return 0;
}

static int maxim_dsm_preinit(struct smart_amp_mod_data_base *mod)
{
	struct smart_amp_mod_struct_t *hspk = (struct smart_amp_mod_struct_t *)mod;

	/* Bitwidth information is not available. Use 16bit as default.
	 * Re-initialize in the prepare function if ncessary
	 */
	hspk->bitwidth = 16;
	return maxim_dsm_init(hspk);
}

static int maxim_dsm_query_memblk_size(struct smart_amp_mod_data_base *mod,
				       enum smart_amp_mod_memblk blk)
{
	struct smart_amp_mod_struct_t *hspk = (struct smart_amp_mod_struct_t *)mod;
	int ret;

	switch (blk) {
	case MOD_MEMBLK_PRIVATE:
		/* Memory size for private data block - dsmhandle */
		ret = maxim_dsm_get_handle_size(hspk);
		if (ret <= 0)
			comp_err(mod->dev, "[DSM] Get handle size error");
		break;
	case MOD_MEMBLK_FRAME:
		/* Memory size for frame buffer block - smart_amp_buf_struct_t */
		/* smart_amp_buf_struct_t -> input, output, voltage, current */
		ret = 4 * DSM_FF_BUF_SZ * sizeof(int32_t);
		/* smart_amp_buf_struct_t -> ff, ff_out, fb */
		ret += 2 * DSM_FF_BUF_DB_SZ * sizeof(int32_t) + DSM_FB_BUF_DB_SZ * sizeof(int32_t);
		break;
	case MOD_MEMBLK_PARAM:
		/* Memory size for param blob block - caldata */
		/* Get the max. number of parameter to allocate memory for model data */
		ret = maxim_dsm_get_num_param(hspk);
		if (ret < 0) {
			comp_err(mod->dev, "[DSM] Get parameter size error");
			return -EINVAL;
		}
		hspk->param.max_param = ret;
		ret = hspk->param.max_param * DSM_SINGLE_PARAM_SZ * sizeof(int32_t);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int maxim_dsm_set_memblk(struct smart_amp_mod_data_base *mod,
				enum smart_amp_mod_memblk blk,
				struct smart_amp_buf *buf)
{
	struct smart_amp_mod_struct_t *hspk = (struct smart_amp_mod_struct_t *)mod;
	int32_t *mem_ptr;

	switch (blk) {
	case MOD_MEMBLK_PRIVATE:
		/* Assign memory to private data */
		hspk->dsmhandle = buf->data;
		bzero(hspk->dsmhandle, buf->size);
		break;
	case MOD_MEMBLK_FRAME:
		/* Assign memory to frame buffers */
		mem_ptr = (int32_t *)buf->data;
		hspk->buf.input = mem_ptr;
		mem_ptr += DSM_FF_BUF_SZ;
		hspk->buf.output = mem_ptr;
		mem_ptr += DSM_FF_BUF_SZ;
		hspk->buf.voltage = mem_ptr;
		mem_ptr += DSM_FF_BUF_SZ;
		hspk->buf.current = mem_ptr;
		mem_ptr += DSM_FF_BUF_SZ;
		hspk->buf.ff.buf = mem_ptr;
		mem_ptr += DSM_FF_BUF_DB_SZ;
		hspk->buf.ff_out.buf = mem_ptr;
		mem_ptr += DSM_FF_BUF_DB_SZ;
		hspk->buf.fb.buf = mem_ptr;
		break;
	case MOD_MEMBLK_PARAM:
		/* Assign memory to config caldata */
		hspk->param.caldata.data = buf->data;
		hspk->param.caldata.data_size = buf->size;
		bzero(hspk->param.caldata.data, hspk->param.caldata.data_size);
		hspk->param.caldata.data_pos = 0;

		/* update full parameter values */
		if (maxim_dsm_get_all_param(hspk) < 0)
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int maxim_dsm_get_supported_fmts(struct smart_amp_mod_data_base *mod,
					const uint16_t **mod_fmts, int *num_mod_fmts)
{
	*num_mod_fmts = supported_fmt_count;
	*mod_fmts = supported_fmts;
	return 0;
}

static int maxim_dsm_set_fmt(struct smart_amp_mod_data_base *mod, uint16_t mod_fmt)
{
	struct smart_amp_mod_struct_t *hspk = (struct smart_amp_mod_struct_t *)mod;
	int ret;

	comp_dbg(mod->dev, "[DSM] smart_amp_mod_set_fmt(): %u", mod_fmt);

	hspk->bitwidth = get_sample_bitdepth(mod_fmt);

	ret = maxim_dsm_init(hspk);
	if (ret) {
		comp_err(mod->dev, "[DSM] Re-initialization error.");
		goto error;
	}
	ret = maxim_dsm_restore_param(hspk);
	if (ret) {
		comp_err(mod->dev, "[DSM] Restoration error.");
		goto error;
	}

error:
	maxim_dsm_flush(hspk);
	return ret;
}

static int maxim_dsm_reset(struct smart_amp_mod_data_base *mod)
{
	/* no-op for reset */
	return 0;
}

static const struct inner_model_ops maxim_dsm_ops = {
	.init = maxim_dsm_preinit,
	.query_memblk_size = maxim_dsm_query_memblk_size,
	.set_memblk = maxim_dsm_set_memblk,
	.get_supported_fmts = maxim_dsm_get_supported_fmts,
	.set_fmt = maxim_dsm_set_fmt,
	.ff_proc = maxim_dsm_ff_proc,
	.fb_proc = maxim_dsm_fb_proc,
	.set_config = maxim_dsm_set_config,
	.get_config = maxim_dsm_get_config,
	.reset = maxim_dsm_reset
};

/**
 * mod_data_create() implementation.
 */

struct smart_amp_mod_data_base *mod_data_create(const struct comp_dev *dev)
{
	struct smart_amp_mod_struct_t *hspk;

	hspk = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*hspk));
	if (!hspk)
		return NULL;

	hspk->base.dev = dev;
	hspk->base.mod_ops = &maxim_dsm_ops;
	return &hspk->base;
}
