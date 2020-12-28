// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Johny Lin <johnylin@google.com>

#include <ipc/stream.h>
#include <sof/audio/codec_adapter/codec/generic.h>
#include <sof/audio/codec_adapter/codec/dcblock.h>
#include <sof/audio/format.h>

/*****************************************************************************/
/* DCBlock processing functions						     */
/*****************************************************************************/

static int32_t dcblock_generic(struct dcblock_state *state,
			       int64_t R, int32_t x)
{
	/*
	 * R: Q2.30, y_prev: Q1.31
	 * R * y_prev: Q3.61
	 */
	int64_t out = ((int64_t)x) - state->x_prev +
		      Q_SHIFT_RND(R * state->y_prev, 61, 31);

	state->y_prev = sat_int32(out);
	state->x_prev = x;

	return state->y_prev;
}

#if CONFIG_FORMAT_S16LE
static void dcblock_s16_default(const struct comp_dev *dev,
				const void *in_buff,
				const void *out_buff,
				uint32_t avail_bytes,
				uint32_t *produced_bytes)
{
	struct comp_data *ca_cd = comp_get_drvdata(dev);
	uint32_t nch = ca_cd->ca_config.channels;
	struct codec_data *codec = comp_get_codec(dev);
	struct dcblock_codec_data *cd = codec->private;
	uint32_t frames = avail_bytes >> 1;
	int16_t *source = (int16_t *)in_buff;
	int16_t *sink = (int16_t *)out_buff;
	struct dcblock_state *state;
	int16_t *x;
	int16_t *y;
	int32_t R;
	int32_t tmp;
	int idx;
	int ch;
	int i;

	for (ch = 0; ch < nch; ch++) {
		state = &cd->state[ch];
		R = cd->R_coeffs[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = source + idx;
			y = sink + idx;
			tmp = dcblock_generic(state, R, *x << 16);
			*y = sat_int16(Q_SHIFT_RND(tmp, 31, 15));
			idx += nch;
		}
	}

	*produced_bytes = avail_bytes;
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void dcblock_s24_default(const struct comp_dev *dev,
				const void *in_buff,
				const void *out_buff,
				uint32_t avail_bytes,
				uint32_t *produced_bytes)
{
	struct comp_data *ca_cd = comp_get_drvdata(dev);
	uint32_t nch = ca_cd->ca_config.channels;
	struct codec_data *codec = comp_get_codec(dev);
	struct dcblock_codec_data *cd = codec->private;
	uint32_t frames = avail_bytes >> 2;
	int32_t *source = (int32_t *)in_buff;
	int32_t *sink = (int32_t *)out_buff;
	struct dcblock_state *state;
	int32_t *x;
	int32_t *y;
	int32_t R;
	int32_t tmp;
	int idx;
	int ch;
	int i;

	for (ch = 0; ch < nch; ch++) {
		state = &cd->state[ch];
		R = cd->R_coeffs[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = source + idx;
			y = sink + idx;
			tmp = dcblock_generic(state, R, *x << 8);
			*y = sat_int24(Q_SHIFT_RND(tmp, 31, 23));
			idx += nch;
		}
	}

	*produced_bytes = avail_bytes;
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void dcblock_s32_default(const struct comp_dev *dev,
				const void *in_buff,
				const void *out_buff,
				uint32_t avail_bytes,
				uint32_t *produced_bytes)
{
	struct comp_data *ca_cd = comp_get_drvdata(dev);
	uint32_t nch = ca_cd->ca_config.channels;
	struct codec_data *codec = comp_get_codec(dev);
	struct dcblock_codec_data *cd = codec->private;
	uint32_t frames = avail_bytes >> 2;
	int32_t *source = (int32_t *)in_buff;
	int32_t *sink = (int32_t *)out_buff;
	struct dcblock_state *state;
	int32_t *x;
	int32_t *y;
	int32_t R;
	int idx;
	int ch;
	int i;

	for (ch = 0; ch < nch; ch++) {
		state = &cd->state[ch];
		R = cd->R_coeffs[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = source + idx;
			y = sink + idx;
			*y = dcblock_generic(state, R, *x);
			idx += nch;
		}
	}

	*produced_bytes = avail_bytes;
}
#endif /* CONFIG_FORMAT_S32LE */

/** \brief DC Blocking Filter processing functions map item. */
struct dcblock_func_map {
	enum sof_ipc_frame src_fmt; /**< source frame format */
	dcblock_func func; /**< processing function */
};

/** \brief Map of formats with dedicated processing functions. */
static const struct dcblock_func_map dcblock_fnmap[] = {
/* { SOURCE_FORMAT , PROCESSING FUNCTION } */
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, dcblock_s16_default },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, dcblock_s24_default },
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, dcblock_s32_default },
#endif /* CONFIG_FORMAT_S32LE */
};

/** \brief Number of processing functions. */
static const size_t dcblock_fncount = ARRAY_SIZE(dcblock_fnmap);

/**
 * \brief Retrieves a DC Blocking processing function matching
 *        the source buffer's frame format.
 * \param src_fmt the frames' format of the source buffer
 */
static dcblock_func dcblock_find_func(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < dcblock_fncount; i++) {
		if (src_fmt == dcblock_fnmap[i].src_fmt)
			return dcblock_fnmap[i].func;
	}

	return NULL;
}

/*****************************************************************************/
/* DCBlock interfaces							     */
/*****************************************************************************/
static int apply_config(struct comp_dev *dev, enum codec_cfg_type type)
{
	int ret = 0;
	int size;
	struct codec_config *cfg;
	void *data;
	struct codec_param *param;
	struct codec_data *codec = comp_get_codec(dev);
	struct dcblock_codec_data *cd = codec->private;
	size_t req_size;

	comp_info(dev, "dcblock: apply_config() type %d", type);

	cfg = (type == CODEC_CFG_SETUP) ? &codec->s_cfg :
					  &codec->r_cfg;
	data = cfg->data;
	size = cfg->size;

	if (!cfg->avail || !size) {
		comp_err(dev, "apply_config() error: no config available, requested conf. type %d",
			 type);
		ret = -EIO;
		goto ret;
	}

	/* Read parameters stored in `data` - it may keep plenty of
	 * parameters. The `size` variable is equal to param->size * count,
	 * where count is number of parameters stored in `data`.
	 */
	while (size > 0) {
		param = data;
		comp_dbg(dev, "apply_config() applying param %d size %d value[0] %d",
			 param->id, param->size, param->data[0]);
		/* Set read parameter */
		if (param->id == DCBLOCK_CONFIG_R_COEFFS) {
			req_size = sizeof(cd->R_coeffs);
			if (param->size < req_size + sizeof(param->id) + sizeof(param->size)) {
				comp_err(dev, "apply_config() error: parameter size %d not enough, required %d",
					 param->size, req_size);
				ret = -EINVAL;
				goto ret;
			}
			ret = memcpy_s(cd->R_coeffs, req_size, param->data, req_size);
			if (ret != 0)
				goto ret;
		} else {
			comp_err(dev, "apply_config() error: parameter id %d not exists", param->id);
			ret = -EINVAL;
			goto ret;
		}
		/* Obtain next parameter, it starts right after the preceding one */
		data = (char *)data + param->size;
		size -= param->size;
	}

	comp_dbg(dev, "apply_config() done");
ret:
	return ret;
}

int dcblock_codec_init(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct dcblock_codec_data *cd = NULL;

	comp_info(dev, "dcblock_codec_init()");

	cd = codec_allocate_memory(dev, sizeof(struct dcblock_codec_data), 0);
	if (!cd) {
		comp_err(dev, "dcblock_codec_init(): failed to allocate memory for dcblock codec data");
		return -ENOMEM;
	}

	codec->private = cd;

	memset(cd->state, 0, sizeof(cd->state));
	cd->dcblock_func = NULL;

	comp_dbg(dev, "dcblock_codec_init() done");

	return 0;
}

int dcblock_codec_prepare(struct comp_dev *dev)
{
	int ret;
	struct comp_data *ca_cd = comp_get_drvdata(dev);
	struct codec_data *codec = comp_get_codec(dev);
	struct dcblock_codec_data *cd = codec->private;
	enum sof_ipc_frame source_format;
	uint32_t period_bytes;

	comp_info(dev, "dcblock_codec_prepare()");

	if (codec->state == CODEC_PREPARED)
		return 0;

	if (!codec->s_cfg.avail && !codec->s_cfg.size) {
		comp_err(dev, "dcblock_codec_prepare() no setup configuration available!");
		return -EIO;
	} else if (!codec->s_cfg.avail) {
		comp_warn(dev, "dcblock_codec_prepare(): no new setup configuration available, using the old one");
		codec->s_cfg.avail = true;
	}
	ret = apply_config(dev, CODEC_CFG_SETUP);
	if (ret) {
		comp_err(dev, "dcblock_codec_prepare() error %x: failed to applay setup config",
			 ret);
		return ret;
	}
	/* Do not reset nor free codec setup config "size" so we can use it
	 * later on in case there is no new one upon reset.
	 */
	codec->s_cfg.avail = false;

	/* get source data format and determine dcblock function */
	source_format = ca_cd->ca_source->stream.frame_fmt;
	cd->dcblock_func = dcblock_find_func(source_format);
	if (!cd->dcblock_func) {
		comp_err(dev, "dcblock_codec_prepare(): no processing function matching frames format");
		return -EINVAL;
	}

	comp_dbg(dev, "dcblock_codec_prepare(): found dcblock_func by source_format = %d", source_format);

	/* setup codec processing data */
	period_bytes = ca_cd->period_bytes;
	codec->cpd.in_buff = codec_allocate_memory(dev, period_bytes, 4);
	if (!codec->cpd.in_buff) {
		comp_err(dev, "dcblock_codec_prepare(): failed to allocate memory for input buffer");
		return -ENOMEM;
	}
	codec->cpd.in_buff_size = period_bytes;

	codec->cpd.out_buff = codec_allocate_memory(dev, period_bytes, 4);
	if (!codec->cpd.out_buff) {
		comp_err(dev, "dcblock_codec_prepare(): failed to allocate memory for output buffer");
		return -ENOMEM;
	}
	codec->cpd.out_buff_size = period_bytes;

	comp_dbg(dev, "dcblock_codec_prepare(): allocated in_buff (size=%d) and out_buff (size=%d) for cpd",
		  codec->cpd.in_buff_size, codec->cpd.out_buff_size);

	comp_dbg(dev, "dcblock_codec_prepare() done");

	return 0;
}

int dcblock_codec_process(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct dcblock_codec_data *cd = codec->private;

	comp_dbg(dev, "dcblock_codec_process()");

	cd->dcblock_func(dev, codec->cpd.in_buff, codec->cpd.out_buff, codec->cpd.avail, &codec->cpd.produced);

	comp_dbg(dev, "dcblock_codec_process() done");

	return 0;
}

int dcblock_codec_apply_config(struct comp_dev *dev)
{
	return apply_config(dev, CODEC_CFG_RUNTIME);
}

int dcblock_codec_reset(struct comp_dev *dev)
{
	struct codec_data *codec = comp_get_codec(dev);
	struct dcblock_codec_data *cd = codec->private;
	memset(cd->state, 0, sizeof(cd->state));
	return 0;
}

int dcblock_codec_free(struct comp_dev *dev)
{
	codec_free_all_memory(dev);
	return 0;
}
