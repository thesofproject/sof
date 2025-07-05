// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intelligo Technology Inc. All rights reserved.
//
// Author: Fu-Yun TSUO <fy.tsuo@intelli-go.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/igo_nr.h>
#include <user/trace.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/igo_nr/igo_nr_comp.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#if CONFIG_IPC_MAJOR_4
#include <ipc4/header.h>
#endif

#define SOF_IGO_NR_MAX_SIZE 4096		/* Max size for coef data in bytes */

enum IGO_NR_ENUM {
	IGO_NR_ONOFF_SWITCH = 0,
	IGO_NR_ENUM_LAST,
};

LOG_MODULE_REGISTER(igo_nr, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(igo_nr);

DECLARE_TR_CTX(igo_nr_tr, SOF_UUID(igo_nr_uuid), LOG_LEVEL_INFO);

static void igo_nr_lib_process(struct comp_data *cd)
{
	/* Pass through the active channel if
	 * 1) It's not enabled, or
	 * 2) hw parameter is not valid.
	 */
	if (!cd->process_enable[cd->config.active_channel_idx] ||
	    cd->invalid_param ||
	    cd->config.igo_params.nr_bypass == 1) {
		memcpy_s(cd->out, IGO_FRAME_SIZE * sizeof(int16_t),
			 cd->in, IGO_FRAME_SIZE * sizeof(int16_t));
	} else {
		IgoLibProcess(cd->p_handle,
			      &cd->igo_stream_data_in,
			      &cd->igo_stream_data_ref,
			      &cd->igo_stream_data_out);
	}
}

#if CONFIG_FORMAT_S16LE
static int igo_nr_capture_s16(struct comp_data *cd,
			      struct sof_source *source,
			      struct sof_sink *sink,
			      int32_t frames)
{
	int16_t *x, *y1, *y2;
	int16_t *x_start, *y_start, *x_end, *y_end;
	int16_t sample;
	size_t x_size, y_size;
	size_t request_size = frames * source_get_frame_bytes(source);
	int sink_samples_without_wrap;
	int samples_without_wrap;
	int samples_left;
	int ret;
	int nch = source_get_channels(source);
	int i, j, k;

#if CONFIG_DEBUG
	int32_t dbg_en = cd->config.igo_params.dump_data == 1 && nch > 1;
	int32_t dbg_ch_idx;

	/* Under DEBUG mode, overwrite the next channel with input interleavedly. */
	if (cd->config.active_channel_idx >= nch)
		dbg_ch_idx = 0;
	else
		dbg_ch_idx = cd->config.active_channel_idx + 1;

#endif

	ret = source_get_data(source, request_size, (void const **)&x,
			      (void const **)&x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer(sink, request_size, (void **)&y1, (void **)&y_start, &y_size);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	y2 = y1;
	x_end = x_start + x_size;
	y_end = y_start + y_size;

	/* Deinterleave the source buffer and keeps the active channel data as input. */
	i = 0;
	samples_left = nch * frames;
	while (samples_left) {
		samples_without_wrap = x_end - x;
		samples_without_wrap = MIN(samples_left, samples_without_wrap);
		sink_samples_without_wrap = y_end - y1;
		samples_without_wrap = MIN(samples_without_wrap, sink_samples_without_wrap);
		for (j = 0; j < samples_without_wrap; j += nch) {
			for (k = 0; k < nch; k++) {
				sample = *x;
				if (k == cd->config.active_channel_idx)
					cd->in[i] = sample;
				else
					*y1 = sample;

				x++;
				y1++;
			}
			i++;
		}

		x = cir_buf_wrap(x, x_start, x_end);
		y1 = cir_buf_wrap(y1, y_start, y_end);
		samples_left -= samples_without_wrap;
	}

	igo_nr_lib_process(cd);

	/* Interleave write the processed data into active output channel. */
	i = 0;
	samples_left = nch * frames;
	while (samples_left) {
		sink_samples_without_wrap = y_end - y2;
		samples_without_wrap = MIN(samples_left, sink_samples_without_wrap);
		for (j = 0; j < samples_without_wrap; j += nch) {
			for (k = 0; k < nch; k++) {
				if (k == cd->config.active_channel_idx)
					*y2 = cd->out[i];
#if CONFIG_DEBUG
				if (dbg_en && k == dbg_ch_idx)
					*y2 = cd->in[i];
#endif
				y2++;
			}
			i++;
		}

		y2 = cir_buf_wrap(y2, y_start, y_end);
		samples_left -= samples_without_wrap;
	}

	source_release_data(source, request_size);
	sink_commit_buffer(sink, request_size);
	return 0;
}
#endif

#if CONFIG_FORMAT_S24LE
static int igo_nr_capture_s24(struct comp_data *cd,
			      struct sof_source *source,
			      struct sof_sink *sink,
			      int32_t frames)
{
	int32_t *x, *y1, *y2;
	int32_t *x_start, *y_start, *x_end, *y_end;
	size_t x_size, y_size;
	size_t request_size = frames * source_get_frame_bytes(source);
	int sink_samples_without_wrap;
	int samples_without_wrap;
	int samples_left;
	int ret;
	int nch = source_get_channels(source);
	int i, j, k;

#if CONFIG_DEBUG
	int32_t dbg_en = cd->config.igo_params.dump_data == 1 && nch > 1;
	int32_t dbg_ch_idx;

	/* Under DEBUG mode, overwrite the next channel with input interleavedly. */
	if (cd->config.active_channel_idx >= nch)
		dbg_ch_idx = 0;
	else
		dbg_ch_idx = cd->config.active_channel_idx + 1;

#endif

	ret = source_get_data(source, request_size, (void const **)&x,
			      (void const **)&x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer(sink, request_size, (void **)&y1, (void **)&y_start, &y_size);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	y2 = y1;
	x_end = x_start + x_size;
	y_end = y_start + y_size;

	/* Deinterleave the source buffer and keeps the active channel data as input. */
	i = 0;
	samples_left = nch * frames;
	while (samples_left) {
		samples_without_wrap = x_end - x;
		samples_without_wrap = MIN(samples_left, samples_without_wrap);
		sink_samples_without_wrap = y_end - y1;
		samples_without_wrap = MIN(samples_without_wrap, sink_samples_without_wrap);
		for (j = 0; j < samples_without_wrap; j += nch) {
			for (k = 0; k < nch; k++) {
				if (k == cd->config.active_channel_idx)
					cd->in[i] = sat_int16(Q_SHIFT_RND(*x, 23, 15));
				else
					*y1 = *x;

				x++;
				y1++;
			}
			i++;
		}

		x = cir_buf_wrap(x, x_start, x_end);
		y1 = cir_buf_wrap(y1, y_start, y_end);
		samples_left -= samples_without_wrap;
	}

	igo_nr_lib_process(cd);

	/* Interleave write the processed data into active output channel. */
	i = 0;
	samples_left = nch * frames;
	while (samples_left) {
		sink_samples_without_wrap = y_end - y2;
		samples_without_wrap = MIN(samples_left, sink_samples_without_wrap);
		for (j = 0; j < samples_without_wrap; j += nch) {
			for (k = 0; k < nch; k++) {
				if (k == cd->config.active_channel_idx)
					*y2 = cd->out[i] << 8;
#if CONFIG_DEBUG
				if (dbg_en && k == dbg_ch_idx)
					*y2 = cd->in[i] << 8;
#endif
				y2++;
			}
			i++;
		}

		y2 = cir_buf_wrap(y2, y_start, y_end);
		samples_left -= samples_without_wrap;
	}

	source_release_data(source, request_size);
	sink_commit_buffer(sink, request_size);
	return 0;
}
#endif

#if CONFIG_FORMAT_S32LE
static int igo_nr_capture_s32(struct comp_data *cd,
			      struct sof_source *source,
			      struct sof_sink *sink,
			      int32_t frames)
{
	int32_t *x, *y1, *y2;
	int32_t *x_start, *y_start, *x_end, *y_end;
	size_t x_size, y_size;
	size_t request_size = frames * source_get_frame_bytes(source);
	int sink_samples_without_wrap;
	int samples_without_wrap;
	int samples_left;
	int ret;
	int nch = source_get_channels(source);
	int i, j, k;

#if CONFIG_DEBUG
	int32_t dbg_en = cd->config.igo_params.dump_data == 1 && nch > 1;
	int32_t dbg_ch_idx;

	/* Under DEBUG mode, overwrite the next channel with input interleavedly. */
	if (cd->config.active_channel_idx >= nch)
		dbg_ch_idx = 0;
	else
		dbg_ch_idx = cd->config.active_channel_idx + 1;

#endif

	ret = source_get_data(source, request_size, (void const **)&x,
			      (void const **)&x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer(sink, request_size, (void **)&y1, (void **)&y_start, &y_size);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	y2 = y1;
	x_end = x_start + x_size;
	y_end = y_start + y_size;

	/* Deinterleave the source buffer and keeps the active channel data as input. */
	i = 0;
	samples_left = nch * frames;
	while (samples_left) {
		samples_without_wrap = x_end - x;
		samples_without_wrap = MIN(samples_left, samples_without_wrap);
		sink_samples_without_wrap = y_end - y1;
		samples_without_wrap = MIN(samples_without_wrap, sink_samples_without_wrap);
		for (j = 0; j < samples_without_wrap; j += nch) {
			for (k = 0; k < nch; k++) {
				if (k == cd->config.active_channel_idx)
					cd->in[i] = sat_int16(Q_SHIFT_RND(*x, 31, 15));
				else
					*y1 = *x;

				x++;
				y1++;
			}
			i++;
		}

		x = cir_buf_wrap(x, x_start, x_end);
		y1 = cir_buf_wrap(y1, y_start, y_end);
		samples_left -= samples_without_wrap;
	}

	igo_nr_lib_process(cd);

	/* Interleave write the processed data into active output channel. */
	i = 0;
	samples_left = nch * frames;
	while (samples_left) {
		sink_samples_without_wrap = y_end - y2;
		samples_without_wrap = MIN(samples_left, sink_samples_without_wrap);
		for (j = 0; j < samples_without_wrap; j += nch) {
			for (k = 0; k < nch; k++) {
				if (k == cd->config.active_channel_idx)
					*y2 = cd->out[i] << 16;
#if CONFIG_DEBUG
				if (dbg_en && k == dbg_ch_idx)
					*y2 = cd->in[i] << 16;
#endif
				y2++;
			}
			i++;
		}

		y2 = cir_buf_wrap(y2, y_start, y_end);
		samples_left -= samples_without_wrap;
	}

	source_release_data(source, request_size);
	sink_commit_buffer(sink, request_size);
	return 0;
}
#endif

static inline int32_t set_capture_func(struct processing_module *mod, struct sof_source *source)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	/* The igo_nr supports S16_LE data. Format converter is needed. */
	switch (source_get_frm_fmt(source)) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		comp_info(dev, "set_capture_func(), SOF_IPC_FRAME_S16_LE");
		cd->igo_nr_func = igo_nr_capture_s16;
		break;
#endif
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		comp_info(dev, "set_capture_func(), SOF_IPC_FRAME_S24_4LE");
		cd->igo_nr_func = igo_nr_capture_s24;
		break;
#endif
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		comp_info(dev, "set_capture_func(), SOF_IPC_FRAME_S32_LE");
		cd->igo_nr_func = igo_nr_capture_s32;
		break;
#endif
	default:
		comp_err(dev, "set_capture_func(), invalid frame_fmt");
		return -EINVAL;
	}
	return 0;
}

static int igo_nr_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &md->cfg;
	struct comp_data *cd;
	size_t bs = cfg->size;
	int32_t ret;

	comp_info(dev, "igo_nr_init()");

	/* Check first that configuration blob size is sane */
	if (bs > SOF_IGO_NR_MAX_SIZE) {
		comp_err(dev, "igo_nr_init() error: configuration blob size = %u > %d",
			 bs, SOF_IGO_NR_MAX_SIZE);
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	ret = IgoLibGetInfo(&cd->igo_lib_info);
	if (ret != IGO_RET_OK) {
		comp_err(dev, "igo_nr_init(): IgoLibGetInfo() Failed.");
		ret = -EINVAL;
		goto cd_fail;
	}

	cd->p_handle = rballoc(SOF_MEM_FLAG_USER, cd->igo_lib_info.handle_size);
	if (!cd->p_handle) {
		comp_err(dev, "igo_nr_init(): igo_handle memory rballoc error for size %d",
			 cd->igo_lib_info.handle_size);
		ret = -ENOMEM;
		goto cd_fail;
	}

	/* Handler for configuration data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "igo_nr_init(): comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto cd_fail2;
	}

	/* Get configuration data */
	ret = comp_init_data_blob(cd->model_handler, bs, cfg->data);
	if (ret < 0) {
		comp_err(dev, "igo_nr_init(): comp_init_data_blob() failed.");
		ret = -ENOMEM;
		goto cd_fail3;
	}

	/* update downstream (playback) or upstream (capture) buffer parameters */
	mod->verify_params_flags = BUFF_PARAMS_RATE;
	comp_info(dev, "igo_nr created");
	return 0;

cd_fail3:
	comp_data_blob_handler_free(cd->model_handler);

cd_fail2:
	rfree(cd->p_handle);

cd_fail:
	rfree(cd);
	return ret;
}

static int igo_nr_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "igo_nr_free()");

	comp_data_blob_handler_free(cd->model_handler);

	rfree(cd->p_handle);
	rfree(cd);
	return 0;
}

/* check component audio stream parameters */
static int32_t igo_nr_check_params(struct processing_module *mod, struct sof_source *source,
				   struct sof_sink *sink)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_info(dev, "igo_nr_check_params()");

	/* set source/sink_frames/rate */
	cd->source_rate = source_get_rate(source);
	cd->sink_rate = sink_get_rate(sink);

	if (source_get_channels(source) != sink_get_channels(sink)) {
		comp_err(dev, "igo_nr_check_params(), mismatch source/sink stream channels");
		cd->invalid_param = true;
	}

	if (!cd->sink_rate) {
		comp_err(dev, "igo_nr_check_params(), zero sink rate");
		return -EINVAL;
	}

	/* The igo_nr supports sample rate 48000 only. */
	switch (cd->source_rate) {
	case 48000:
		comp_info(dev, "igo_nr_check_params(), sample rate = 48000");
		cd->invalid_param = false;
		break;
	default:
		comp_err(dev, "igo_nr_check_params(), invalid sample rate");
		cd->invalid_param = true;
	}

	return cd->invalid_param ? -EINVAL : 0;
}

static int32_t igo_nr_check_config_validity(struct comp_dev *dev,
					    struct comp_data *cd)
{
	struct sof_igo_nr_config *p_config = comp_get_data_blob(cd->model_handler, NULL, NULL);

	if (!p_config) {
		comp_err(dev, "igo_nr_check_config_validity() error: invalid cd->model_handler");
		return -EINVAL;
	} else if (p_config->active_channel_idx >= SOF_IPC_MAX_CHANNELS) {
		comp_err(dev, "igo_nr_check_config_validity() error: invalid active_channel_idxs");
		return -EINVAL;
	} else {
		return 0;
	}
}

static inline void igo_nr_set_chan_process(struct comp_data *cd, int32_t chan)
{
	if (!cd->process_enable[chan])
		cd->process_enable[chan] = true;
}

static inline void igo_nr_set_chan_passthrough(struct comp_data *cd, int32_t chan)
{
	if (cd->process_enable[chan]) {
		cd->process_enable[chan] = false;
		IgoLibInit(cd->p_handle, &cd->igo_lib_config, &cd->config.igo_params);
	}
}

static int igo_nr_get_config(struct processing_module *mod,
			     uint32_t config_id, uint32_t *data_offset_size,
			     uint8_t *fragment, size_t fragment_size)
{
	struct comp_dev *dev = mod->dev;

#if CONFIG_IPC_MAJOR_3
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	int j;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "igo_nr_get_config(), SOF_CTRL_CMD_BINARY");
		return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
	case SOF_CTRL_CMD_SWITCH:
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = cd->process_enable[j];
			comp_info(dev, "igo_nr_get_config(), channel = %u, value = %u",
				  cdata->chanv[j].channel,
				  cdata->chanv[j].value);
		}
		return 0;
	default:
		comp_err(dev, "igo_nr_cmd_get_config() error: invalid cdata->cmd %d", cdata->cmd);
		return -EINVAL;
	}

#elif CONFIG_IPC_MAJOR_4
	comp_err(dev, "igo_nr_cmd_get_config() error: not supported");
	return -EINVAL;
#endif
}

static int32_t igo_nr_set_chan(struct processing_module *mod, void *data)
{
#if CONFIG_IPC_MAJOR_3
	struct sof_ipc_ctrl_data *cdata = data;
#elif CONFIG_IPC_MAJOR_4
	struct sof_ipc4_control_msg_payload *cdata = data;
#endif
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint32_t val;
	int32_t ch;
	int32_t j;

	for (j = 0; j < cdata->num_elems; j++) {
		ch = cdata->chanv[j].channel;
		val = cdata->chanv[j].value;
		comp_info(dev, "igo_nr_cmd_set_value(), channel = %d, value = %u", ch, val);
		if (ch < 0 || ch >= SOF_IPC_MAX_CHANNELS) {
			comp_err(dev, "igo_nr_cmd_set_value(), illegal channel = %d", ch);
			return -EINVAL;
		}

		if (val)
			igo_nr_set_chan_process(cd, ch);
		else
			igo_nr_set_chan_passthrough(cd, ch);
	}

	return 0;
}

static int igo_nr_set_config(struct processing_module *mod, uint32_t param_id,
			     enum module_cfg_fragment_position pos,
			     uint32_t data_offset_size, const uint8_t *fragment,
			     size_t fragment_size, uint8_t *response,
			     size_t response_size)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

	comp_info(dev, "igo_nr_set_config()");

#if CONFIG_IPC_MAJOR_3
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "igo_nr_cmd_set_config(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_set(cd->model_handler, pos, data_offset_size,
					 fragment, fragment_size);
		if (ret >= 0)
			ret = igo_nr_check_config_validity(dev, cd);

		return ret;
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "igo_nr_cmd_set_config(), SOF_CTRL_CMD_SWITCH, cdata->comp_id = %u",
			 cdata->comp_id);
		return igo_nr_set_chan(mod, cdata);

	default:
		comp_err(dev, "igo_nr_cmd_set_config() error: invalid cdata->cmd");
		return -EINVAL;
	}
#elif CONFIG_IPC_MAJOR_4
	struct sof_ipc4_control_msg_payload *ctl = (struct sof_ipc4_control_msg_payload *)fragment;

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
		comp_info(dev, "igo_nr_cmd_set_config(), SOF_IPC4_SWITCH_CONTROL_PARAM_ID");
		return igo_nr_set_chan(mod, ctl);
	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		comp_err(dev, "igo_nr_set_config(), illegal control.");
		return -EINVAL;
	}

	comp_info(dev, "igo_nr_cmd_set_config(), bytes");
	ret = comp_data_blob_set(cd->model_handler, pos, data_offset_size, fragment, fragment_size);
	if (ret >= 0)
		ret = igo_nr_check_config_validity(dev, cd);

	return ret;
#endif
}

static void igo_nr_print_config(struct processing_module *mod)
{
	struct comp_data __maybe_unused *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "  igo_params_ver		%d",
		 cd->config.igo_params.igo_params_ver);
	comp_dbg(dev, "  dump_data			%d",
		 cd->config.igo_params.dump_data);
	comp_dbg(dev, "  nr_bypass			%d",
		 cd->config.igo_params.nr_bypass);
	comp_dbg(dev, "  nr_mode1_en			%d",
		 cd->config.igo_params.nr_mode1_en);
	comp_dbg(dev, "  nr_mode3_en			%d",
		 cd->config.igo_params.nr_mode3_en);
	comp_dbg(dev, "  nr_ul_enable		%d",
		 cd->config.igo_params.nr_ul_enable);
	comp_dbg(dev, "  agc_gain			%d",
		 cd->config.igo_params.agc_gain);
	comp_dbg(dev, "  nr_voice_str		%d",
		 cd->config.igo_params.nr_voice_str);
	comp_dbg(dev, "  nr_level			%d",
		 cd->config.igo_params.nr_level);
	comp_dbg(dev, "  nr_mode1_floor		%d",
		 cd->config.igo_params.nr_mode1_floor);
	comp_dbg(dev, "  nr_mode1_od			%d",
		 cd->config.igo_params.nr_mode1_od);
	comp_dbg(dev, "  nr_mode1_pp_param7		%d",
		 cd->config.igo_params.nr_mode1_pp_param7);
	comp_dbg(dev, "  nr_mode1_pp_param8		%d",
		 cd->config.igo_params.nr_mode1_pp_param8);
	comp_dbg(dev, "  nr_mode1_pp_param10		%d",
		 cd->config.igo_params.nr_mode1_pp_param10);
	comp_dbg(dev, "  nr_mode3_floor		%d",
		 cd->config.igo_params.nr_mode3_floor);
	comp_dbg(dev, "  nr_mode1_pp_param53		%d",
		 cd->config.igo_params.nr_mode1_pp_param53);
	comp_dbg(dev, "  active_channel_idx		%d",
		 cd->config.active_channel_idx);

}

static void igo_nr_set_igo_params(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_igo_nr_config *p_config = comp_get_data_blob(cd->model_handler, NULL, NULL);
	struct comp_dev *dev = mod->dev;

	comp_info(dev, "igo_nr_set_igo_params()");
	igo_nr_check_config_validity(dev, cd);

	if (p_config) {
		comp_info(dev, "New config detected.");
		cd->config = *p_config;
		igo_nr_print_config(mod);
	}
}

/* copy and process stream data from source to sink buffers */
static int igo_nr_process(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_source *source = sources[0];
	struct sof_sink *sink = sinks[0];
	int ret;

	comp_dbg(dev, "igo_nr_copy()");

	/* Process only when frames count is enough. */
	if (source_get_data_frames_available(source) < IGO_FRAME_SIZE ||
	    sink_get_free_frames(sink) < IGO_FRAME_SIZE) {
		comp_warn(dev, "No data to process.");
		return 0;
	}

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler))
		igo_nr_set_igo_params(mod);

	ret = cd->igo_nr_func(cd, source, sink, IGO_FRAME_SIZE);
	if (ret)
		comp_err(dev, "Failed process.");

	return ret;
}

static void igo_nr_lib_init(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	cd->igo_lib_config.algo_name = "igo_nr";
	cd->igo_lib_config.in_ch_num = 1;
	cd->igo_lib_config.ref_ch_num = 0;
	cd->igo_lib_config.out_ch_num = 1;
	IgoLibInit(cd->p_handle, &cd->igo_lib_config, &cd->config.igo_params);

	cd->igo_stream_data_in.data = cd->in;
	cd->igo_stream_data_in.data_width = IGO_DATA_16BIT;
	cd->igo_stream_data_in.sample_num = IGO_FRAME_SIZE;
	cd->igo_stream_data_in.sampling_rate = 48000;

	cd->igo_stream_data_ref.data = NULL;
	cd->igo_stream_data_ref.data_width = 0;
	cd->igo_stream_data_ref.sample_num = 0;
	cd->igo_stream_data_ref.sampling_rate = 0;

	cd->igo_stream_data_out.data = cd->out;
	cd->igo_stream_data_out.data_width = IGO_DATA_16BIT;
	cd->igo_stream_data_out.sample_num = IGO_FRAME_SIZE;
	cd->igo_stream_data_out.sampling_rate = 48000;
}

#if CONFIG_IPC_MAJOR_4
static int igo_nr_ipc4_params(struct processing_module *mod, struct sof_source *source,
			      struct sof_sink *sink)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_dev *dev = mod->dev;
	int ret;

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);
	component_set_nearest_period_frames(dev, params->rate);
	ret = source_set_params(source, params, true);
	if (ret)
		return ret;

	return sink_set_params(sink, params, true);
}
#endif /* CONFIG_IPC_MAJOR_4 */

static int32_t igo_nr_prepare(struct processing_module *mod,
			      struct sof_source **sources, int num_of_sources,
			      struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_source *source = sources[0];
	struct sof_sink *sink = sinks[0];
	int ret;

	comp_dbg(dev, "igo_nr_prepare()");

	if (!source || !sink) {
		comp_err(dev, "no source or sink");
		return -ENOTCONN;
	}

#if CONFIG_IPC_MAJOR_4
	ret = igo_nr_ipc4_params(mod, source, sink);
	if (ret) {
		comp_err(dev, "Failed to set source or and sink parameters.");
		return ret;
	}
#endif

	ret = igo_nr_check_params(mod, source, sink);
	if (ret)
		return ret;

	source_set_alignment_constants(source, 1, IGO_FRAME_SIZE);
	sink_set_alignment_constants(sink, 1, IGO_FRAME_SIZE);

	igo_nr_set_igo_params(mod);

	igo_nr_lib_init(mod);

	comp_dbg(dev, "post igo_nr_lib_init");
	igo_nr_print_config(mod);

	/* Clear in/out buffers */
	memset(cd->in, 0, IGO_NR_IN_BUF_LENGTH * sizeof(int16_t));
	memset(cd->out, 0, IGO_NR_OUT_BUF_LENGTH * sizeof(int16_t));

	/* Default NR on
	 *
	 * Note: There is a race condition with this switch control set and kernel set
	 * ALSA switch control if such is defined in topology. This set overrides kernel
	 * SOF driver set for control since it happens just after component init. The
	 * user needs to re-apply the control to get expected operation. The owner of
	 * this component should check if this is desired operation. A possible fix would
	 * be set here only if kernel has not applied the switch control.
	 */
	cd->process_enable[cd->config.active_channel_idx] = true;

	return set_capture_func(mod, source);
}

static int igo_nr_reset(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "igo_nr_reset()");

	cd->igo_nr_func = NULL;

	cd->source_rate = 0;
	cd->sink_rate = 0;
	cd->invalid_param = false;

	return 0;
}

static const struct module_interface igo_nr_interface = {
	.init = igo_nr_init,
	.prepare = igo_nr_prepare,
	.process = igo_nr_process,
	.set_configuration = igo_nr_set_config,
	.get_configuration = igo_nr_get_config,
	.reset = igo_nr_reset,
	.free = igo_nr_free
};

#if CONFIG_COMP_IGO_NR_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

SOF_LLEXT_MOD_ENTRY(igo_nr, &igo_nr_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("IGO_NR", igo_nr_llext_entry, 1, SOF_REG_UUID(igo_nr), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(igo_nr_interface, igo_nr_uuid, igo_nr_tr);
SOF_MODULE_INIT(igo_nr, sys_comp_module_igo_nr_interface_init);

#endif
