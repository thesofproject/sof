// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intelligo Technology Inc. All rights reserved.
//
// Author: Fu-Yun TSUO <fy.tsuo@intelli-go.com>

#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/igo_nr.h>
#include <user/trace.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/igo_nr/igo_nr_comp.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define SOF_IGO_NR_MAX_SIZE 4096		/* Max size for coef data in bytes */

struct IGO_PARAMS {
	uint32_t igo_params_ver;
	uint32_t dump_data;
	uint32_t nr_bypass;
	uint32_t nr_mode1_en;
	uint32_t nr_mode3_en;
	uint32_t nr_ul_enable;
	uint32_t agc_gain;
	uint32_t nr_voice_str;
	uint32_t nr_level;
	uint32_t nr_mode1_floor;
	uint32_t nr_mode1_od;
	uint32_t nr_mode1_pp_param7;
	uint32_t nr_mode1_pp_param8;
	uint32_t nr_mode1_pp_param10;
	uint32_t nr_mode3_floor;
	uint32_t nr_mode1_pp_param53;
} __attribute__((packed));

enum IGO_NR_ENUM {
	IGO_NR_ONOFF_SWITCH = 0,
	IGO_NR_ENUM_LAST,
};

static const struct comp_driver comp_igo_nr;

/* 696ae2bc-2877-11eb-adc1-0242ac120002 */
DECLARE_SOF_RT_UUID("igo-nr", igo_nr_uuid,  0x696ae2bc, 0x2877, 0x11eb, 0xad, 0xc1,
		    0x02, 0x42, 0xac, 0x12, 0x00, 0x02);

DECLARE_TR_CTX(igo_nr_tr, SOF_UUID(igo_nr_uuid), LOG_LEVEL_INFO);

#if CONFIG_FORMAT_S16LE
static void igo_nr_capture_s16(struct comp_data *cd,
			       const struct audio_stream *source,
			       struct audio_stream *sink,
			       int32_t src_frames,
			       int32_t sink_frames)
{
	int32_t in_nch = source->channels;
	int32_t out_nch = sink->channels;
	int32_t i;
	int32_t idx_in = 0;
	int32_t idx_out = 0;
	int16_t *x;
	int16_t *y;
#if CONFIG_DEBUG
	int32_t dbg_en = cd->config.igo_params.dump_data == 1 && out_nch > 1;
#endif

	/* Deinterleave the source buffer and keeps the first channel data as input. */
	for (i = 0; i < src_frames; i++) {
		x = audio_stream_read_frag_s16(source, idx_in);
		cd->in[cd->in_wpt] = (*x);

		cd->in_wpt++;
		idx_in += in_nch;
	}

	/* Interleave write the processed data into first channel of output buffer.
	 * Under DEBUG mode, write the input data into second channel interleavedly.
	 */
	for (i = 0; i < sink_frames; i++) {
		y = audio_stream_write_frag_s32(sink, idx_out);
		*y = (cd->out[cd->out_rpt]);
#if CONFIG_DEBUG
		if (dbg_en) {
			y = audio_stream_write_frag_s32(sink, idx_out + 1);
			*y = (cd->in[cd->out_rpt]);
		}
#endif
		cd->out_rpt++;
		idx_out += out_nch;
	}
}
#endif

#if CONFIG_FORMAT_S24LE
static void igo_nr_capture_s24(struct comp_data *cd,
			       const struct audio_stream *source,
			       struct audio_stream *sink,
			       int32_t src_frames,
			       int32_t sink_frames)
{
	int32_t in_nch = source->channels;
	int32_t out_nch = sink->channels;
	int32_t i;
	int32_t idx_in = 0;
	int32_t idx_out = 0;
	int32_t *x;
	int32_t *y;
#if CONFIG_DEBUG
	int32_t dbg_en = cd->config.igo_params.dump_data == 1 && out_nch > 1;
#endif

	/* Deinterleave the source buffer and keeps the first channel data as input. */
	for (i = 0; i < src_frames; i++) {
		x = audio_stream_read_frag_s32(source, idx_in);
		cd->in[cd->in_wpt] = Q_SHIFT_RND(*x, 24, 16);

		cd->in_wpt++;
		idx_in += in_nch;
	}

	/* Interleave write the processed data into first channel of output buffer.
	 * Under DEBUG mode, write the input data into second channel interleavedly.
	 */
	for (i = 0; i < sink_frames; i++) {
		y = audio_stream_write_frag_s32(sink, idx_out);
		*y = (cd->out[cd->out_rpt]) << 8;

#if CONFIG_DEBUG
		if (dbg_en) {
			y = audio_stream_write_frag_s32(sink, idx_out + 1);
			*y = (cd->in[cd->out_rpt]) << 8;
		}
#endif
		cd->out_rpt++;
		idx_out += out_nch;
	}
}
#endif

#if CONFIG_FORMAT_S32LE
static void igo_nr_capture_s32(struct comp_data *cd,
			       const struct audio_stream *source,
			       struct audio_stream *sink,
			       int32_t src_frames,
			       int32_t sink_frames)
{
	int32_t in_nch = source->channels;
	int32_t out_nch = sink->channels;
	int32_t i;
	int32_t idx_in = 0;
	int32_t idx_out = 0;
	int32_t *x;
	int32_t *y;
#if CONFIG_DEBUG
	int32_t dbg_en = cd->config.igo_params.dump_data == 1 && out_nch > 1;
#endif

	for (i = 0; i < src_frames; i++) {
		x = audio_stream_read_frag_s32(source, idx_in);
		cd->in[cd->in_wpt] = Q_SHIFT_RND(*x, 32, 16);

		cd->in_wpt++;
		idx_in += in_nch;
	}

	/* Deinterleave the source buffer and keeps the first channel data as input. */
	for (i = 0; i < sink_frames; i++) {
		y = audio_stream_write_frag_s32(sink, idx_out);
		*y = (cd->out[cd->out_rpt]) << 16;

	/* Interleave write the processed data into first channel of output buffer.
	 * Under DEBUG mode, write the input data into second channel interleavedly.
	 */
#if CONFIG_DEBUG
		if (dbg_en) {
			y = audio_stream_write_frag_s32(sink, idx_out + 1);
			*y = (cd->in[cd->out_rpt]) << 16;
		}
#endif
		cd->out_rpt++;
		idx_out += out_nch;
	}
}
#endif

static inline int32_t set_capture_func(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	/* The igo_nr supports S16_LE data. Format converter is needed. */
	switch (sourceb->stream.frame_fmt) {
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

static struct comp_dev *igo_nr_new(const struct comp_driver *drv,
				   struct sof_ipc_comp *comp)
{
	struct sof_ipc_comp_process *ipc_igo_nr =
		(struct sof_ipc_comp_process *)comp;
	struct comp_dev *dev;
	struct comp_data *cd = NULL;
	size_t bs = ipc_igo_nr->size;
	int32_t ret;

	comp_cl_info(&comp_igo_nr, "igo_nr_new()");

	/* Check first that configuration blob size is sane */
	if (bs > SOF_IGO_NR_MAX_SIZE) {
		comp_cl_err(&comp_igo_nr, "igo_nr_new() error: configuration blob size = %u > %d",
			    bs, SOF_IGO_NR_MAX_SIZE);
		return NULL;
	}

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	memcpy_s(COMP_GET_IPC(dev, sof_ipc_comp_process),
		 sizeof(struct sof_ipc_comp_process), ipc_igo_nr,
		 sizeof(struct sof_ipc_comp_process));

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		goto err;

	ret = IgoLibGetInfo(&cd->igo_lib_info);
	if (ret != IGO_RET_OK) {
		comp_cl_err(&comp_igo_nr, "igo_nr_new(): IgoLibGetInfo() Failed.");
		goto err;
	}

	cd->p_handle = rballoc(0, SOF_MEM_CAPS_RAM, cd->igo_lib_info.handle_size);
	if (!cd->p_handle) {
		comp_cl_err(&comp_igo_nr, "igo_nr_new(): igo_handle memory rballoc error");
		goto err;
	}

	comp_set_drvdata(dev, cd);

	/* Handler for configuration data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_cl_err(&comp_igo_nr, "igo_nr_new(): comp_data_blob_handler_new() failed.");
		goto err;
	}

	/* Get configuration data */
	ret = comp_init_data_blob(cd->model_handler, bs, ipc_igo_nr->data);
	if (ret < 0) {
		comp_cl_err(&comp_igo_nr, "igo_nr_new(): comp_init_data_blob() failed.");
		goto err;
	}
	dev->state = COMP_STATE_READY;

	comp_cl_info(&comp_igo_nr, "igo_nr created");

	return dev;

err:
	rfree(dev);
	rfree(cd);
	return NULL;
}

static void igo_nr_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "igo_nr_free()");

	comp_data_blob_handler_free(cd->model_handler);

	rfree(cd->p_handle);
	rfree(cd);
	rfree(dev);
}

static int32_t igo_nr_verify_params(struct comp_dev *dev,
				    struct sof_ipc_stream_params *params)
{
	int32_t ret;

	comp_dbg(dev, "igo_nr_verify_params()");

	/* update downstream (playback) or upstream (capture) buffer parameters
	 */
	ret = comp_verify_params(dev, BUFF_PARAMS_RATE, params);
	if (ret < 0)
		comp_err(dev, "igo_nr_verify_params(): comp_verify_params() failed.");

	return ret;
}

/* set component audio stream parameters */
static int32_t igo_nr_params(struct comp_dev *dev,
			     struct sof_ipc_stream_params *pcm_params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sinkb;
	struct comp_buffer *sourceb;
	int32_t err;

	comp_info(dev, "igo_nr_params()");

	err = igo_nr_verify_params(dev, pcm_params);
	if (err < 0) {
		comp_err(dev, "igo_nr_params(): pcm params verification failed.");
		return err;
	}

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* set source/sink_frames/rate */
	cd->source_rate = sourceb->stream.rate;
	cd->sink_rate = sinkb->stream.rate;
	if (!cd->sink_rate) {
		comp_err(dev, "igo_nr_params(), zero sink rate");
		return -EINVAL;
	}

	cd->sink_frames = dev->frames;
	cd->source_frames = ceil_divide(dev->frames * cd->source_rate,
					cd->sink_rate);

	/* Use empirical add +10 for target frames number to avoid xruns and
	 * distorted audio in beginning of streaming. It's slightly more than
	 * min. needed and does not increase much peak load and buffer memory
	 * consumption. The copy() function will find the limits and process
	 * less frames after the buffers levels stabilize.
	 */
	cd->source_frames_max = cd->source_frames + 10;
	cd->sink_frames_max = cd->sink_frames + 10;
	cd->frames = MAX(cd->source_frames_max, cd->sink_frames_max);

	comp_dbg(dev, "igo_nr_params(), source_rate=%u, sink_rate=%u, source_frames_max=%d, sink_frames_max=%d",
		 cd->source_rate, cd->sink_rate,
		 cd->source_frames_max, cd->sink_frames_max);

	/* The igo_nr supports sample rate 16000 only. */
	switch (sourceb->stream.rate) {
	case 16000:
		comp_info(dev, "igo_nr_params(), sample rate = 16000");
		cd->invalid_param = false;
		break;
	default:
		comp_err(dev, "igo_nr_params(), invalid sample rate");
		cd->invalid_param = true;
		return -EINVAL;
	}

	return 0;
}

static inline void igo_nr_set_chan_process(struct comp_dev *dev, int32_t chan)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	if (!cd->process_enable[chan])
		cd->process_enable[chan] = true;
}

static inline void igo_nr_set_chan_passthrough(struct comp_dev *dev, int32_t chan)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	if (cd->process_enable[chan]) {
		cd->process_enable[chan] = false;
		IgoLibInit(cd->p_handle, &cd->igo_lib_config, &cd->config.igo_params);
	}
}

static int32_t igo_nr_cmd_get_data(struct comp_dev *dev,
				   struct sof_ipc_ctrl_data *cdata,
				   int32_t max_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t ret;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "igo_nr_cmd_get_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_get_cmd(cd->model_handler, cdata, max_size);
		break;
	default:
		comp_err(dev, "igo_nr_cmd_get_data() error: invalid cdata->cmd", cdata->cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int32_t igo_nr_cmd_get_value(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t j;
	int32_t ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_info(dev, "igo_nr_cmd_get_value(), SOF_CTRL_CMD_ENUM index=%d", cdata->index);
		switch (cdata->index) {
		case IGO_NR_ONOFF_SWITCH:
			for (j = 0; j < cdata->num_elems; j++)
				cdata->chanv[j].value = cd->process_enable[j];
			break;
		default:
			comp_err(dev, "igo_nr_cmd_get_value() error: invalid cdata->index %d",
				 cdata->index);
			ret = -EINVAL;
			break;
		}
		break;
	case SOF_CTRL_CMD_SWITCH:
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = cd->process_enable[j];
			comp_info(dev, "igo_nr_cmd_get_value(), channel = %u, value = %u",
				  cdata->chanv[j].channel,
				  cdata->chanv[j].value);
		}
		break;
	default:
		comp_err(dev, "igo_nr_cmd_get_value() error: invalid cdata->cmd %d", cdata->cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int32_t igo_nr_cmd_set_data(struct comp_dev *dev,
				   struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t ret;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "igo_nr_cmd_set_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_set_cmd(cd->model_handler, cdata);
		break;
	default:
		comp_err(dev, "igo_nr_cmd_set_data() error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int32_t igo_nr_set_chan(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
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
			igo_nr_set_chan_process(dev, ch);
		else
			igo_nr_set_chan_passthrough(dev, ch);
	}

	return 0;
}

static int32_t igo_nr_cmd_set_value(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	int32_t ret;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_dbg(dev, "igo_nr_cmd_set_value(), SOF_CTRL_CMD_ENUM index=%d", cdata->index);
		switch (cdata->index) {
		case IGO_NR_ONOFF_SWITCH:
			ret = igo_nr_set_chan(dev, cdata);
			break;
		default:
			comp_err(dev, "igo_nr_cmd_set_value() error: invalid cdata->index %d",
				 cdata->index);
			ret = -EINVAL;
			break;
		}
		break;
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "igo_nr_cmd_set_value(), SOF_CTRL_CMD_SWITCH, cdata->comp_id = %u",
			 cdata->comp_id);
		ret = igo_nr_set_chan(dev, cdata);
		break;

	default:
		comp_err(dev, "igo_nr_cmd_set_value() error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int32_t igo_nr_cmd(struct comp_dev *dev,
			  int32_t cmd,
			  void *data,
			  int32_t max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;

	comp_info(dev, "igo_nr_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		return igo_nr_cmd_set_data(dev, cdata);
	case COMP_CMD_GET_DATA:
		return igo_nr_cmd_get_data(dev, cdata, max_data_size);
	case COMP_CMD_SET_VALUE:
		return igo_nr_cmd_set_value(dev, cdata);
	case COMP_CMD_GET_VALUE:
		return igo_nr_cmd_get_value(dev, cdata);
	default:
		comp_err(dev, "igo_nr_cmd() error: invalid command");
		return -EINVAL;
	}
}

static void igo_nr_process(struct comp_dev *dev,
			   struct comp_buffer *source,
			   struct comp_buffer *sink,
			   int32_t src_frames,
			   int32_t sink_frames,
			   uint32_t source_bytes,
			   uint32_t sink_bytes)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	buffer_invalidate(source, source_bytes);

	cd->igo_nr_func(cd, &source->stream, &sink->stream, src_frames, sink_frames);

	buffer_writeback(sink, sink_bytes);

	/* calc new free and available */
	comp_update_buffer_consume(source, source_bytes);
	comp_update_buffer_produce(sink, sink_bytes);
}

static void igo_nr_get_igo_params(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	struct sof_igo_nr_config *p_config = comp_get_data_blob(cd->model_handler, NULL, NULL);
	comp_info(dev, "igo_nr_get_igo_params()");

	if (p_config) {
		cd->config = *p_config;

		comp_dbg(dev, "config changed");
		comp_dbg(dev, "  igo_params_ver       %d",
			 cd->config.igo_params.igo_params_ver);
		comp_dbg(dev, "  dump_data:           %d",
			 cd->config.igo_params.dump_data);
		comp_dbg(dev, "  nr_bypass:           %d",
			 cd->config.igo_params.nr_bypass);
		comp_dbg(dev, "  nr_mode1_en          %d",
			 cd->config.igo_params.nr_mode1_en);
		comp_dbg(dev, "  nr_mode3_en          %d",
			 cd->config.igo_params.nr_mode3_en);
		comp_dbg(dev, "  nr_ul_enable         %d",
			 cd->config.igo_params.nr_ul_enable);
		comp_dbg(dev, "  agc_gain             %d",
			 cd->config.igo_params.agc_gain);
		comp_dbg(dev, "  nr_voice_str         %d",
			 cd->config.igo_params.nr_voice_str);
		comp_dbg(dev, "  nr_level             %d",
			 cd->config.igo_params.nr_level);
		comp_dbg(dev, "  nr_mode1_floor       %d",
			 cd->config.igo_params.nr_mode1_floor);
		comp_dbg(dev, "  nr_mode1_od          %d",
			 cd->config.igo_params.nr_mode1_od);
		comp_dbg(dev, "  nr_mode1_pp_param7   %d",
			 cd->config.igo_params.nr_mode1_pp_param7);
		comp_dbg(dev, "  nr_mode1_pp_param8   %d",
			 cd->config.igo_params.nr_mode1_pp_param8);
		comp_dbg(dev, "  nr_mode1_pp_param10  %d",
			 cd->config.igo_params.nr_mode1_pp_param10);
		comp_dbg(dev, "  nr_mode3_floor       %d",
			 cd->config.igo_params.nr_mode3_floor);
		comp_dbg(dev, "  nr_mode1_pp_param53  %d",
			 cd->config.igo_params.nr_mode1_pp_param53);
	}
}

/* copy and process stream data from source to sink buffers */
static int32_t igo_nr_copy(struct comp_dev *dev)
{
	struct comp_copy_limits cl;
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t src_frames;
	int32_t sink_frames;

	comp_dbg(dev, "igo_nr_copy()");

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler))
		igo_nr_get_igo_params(dev);

	/* Get source, sink, number of frames etc. to process. */
	comp_get_copy_limits(sourceb, sinkb, &cl);

	src_frames = audio_stream_get_avail_frames(&sourceb->stream);
	sink_frames = audio_stream_get_free_frames(&sinkb->stream);

	comp_dbg(dev, "src_frames = %d, sink_frames = %d.", src_frames, sink_frames);

	/* Run the data consume/produce below */
	src_frames = MIN(src_frames, IGO_FRAME_SIZE - cd->in_wpt);
	sink_frames = MIN(sink_frames, IGO_FRAME_SIZE - cd->out_rpt);

	comp_dbg(dev, "consumed src_frames = %d, produced sink_frames = %d.",
		 src_frames, sink_frames);

	igo_nr_process(dev, sourceb, sinkb, src_frames, sink_frames,
		       src_frames * cl.source_frame_bytes,
		       sink_frames * cl.sink_frame_bytes);

    /* Run algorithm if buffers are available */
	if (cd->in_wpt == IGO_FRAME_SIZE && cd->out_rpt == IGO_FRAME_SIZE) {
		if (!cd->process_enable[0] || cd->invalid_param) {
			memcpy_s(cd->out, IGO_FRAME_SIZE * sizeof(int16_t),
				 cd->in, IGO_FRAME_SIZE * sizeof(int16_t));
		} else {
			IgoLibProcess(cd->p_handle,
				      &cd->igo_stream_data_in,
				      &cd->igo_stream_data_ref,
				      &cd->igo_stream_data_out);
		}

		cd->in_wpt = 0;
		cd->out_rpt = 0;
	}

	return 0;
}

static void igo_nr_lib_init(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	cd->igo_lib_config.algo_name = "igo_nr";
	cd->igo_lib_config.in_ch_num = 1;
	cd->igo_lib_config.ref_ch_num = 0;
	cd->igo_lib_config.out_ch_num = 1;
	IgoLibInit(cd->p_handle, &cd->igo_lib_config, &cd->config.igo_params);

	cd->igo_stream_data_in.data = cd->in;
	cd->igo_stream_data_in.data_width = IGO_DATA_16BIT;
	cd->igo_stream_data_in.sample_num = IGO_FRAME_SIZE;
	cd->igo_stream_data_in.sampling_rate = 16000;

	cd->igo_stream_data_ref.data = NULL;
	cd->igo_stream_data_ref.data_width = 0;
	cd->igo_stream_data_ref.sample_num = 0;
	cd->igo_stream_data_ref.sampling_rate = 0;

	cd->igo_stream_data_out.data = cd->out;
	cd->igo_stream_data_out.data_width = IGO_DATA_16BIT;
	cd->igo_stream_data_out.sample_num = IGO_FRAME_SIZE;
	cd->igo_stream_data_out.sampling_rate = 16000;
}

static int32_t igo_nr_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t ret;

	comp_dbg(dev, "igo_nr_prepare()");

	igo_nr_get_igo_params(dev);

	igo_nr_lib_init(dev);

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* Clear in/out buffers */
	memset(cd->in, 0, IGO_NR_IN_BUF_LENGTH * sizeof(int16_t));
	memset(cd->out, 0, IGO_NR_OUT_BUF_LENGTH * sizeof(int16_t));
	cd->in_wpt = 0;
	cd->out_rpt = 0;

	/* Default NR on */
	cd->process_enable[0] = true;

	return set_capture_func(dev);
}

static int32_t igo_nr_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "igo_nr_reset()");

	cd->igo_nr_func = NULL;

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static int32_t igo_nr_trigger(struct comp_dev *dev, int32_t cmd)
{
	int32_t ret;

	comp_info(dev, "igo_nr_trigger(), command = %u", cmd);

	ret = comp_set_state(dev, cmd);
	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		ret = PPL_STATUS_PATH_STOP;

	return ret;
}

static const struct comp_driver comp_igo_nr = {
	.uid = SOF_RT_UUID(igo_nr_uuid),
	.tctx	= &igo_nr_tr,
	.ops = {
		.create = igo_nr_new,
		.free = igo_nr_free,
		.params = igo_nr_params,
		.cmd = igo_nr_cmd,
		.copy = igo_nr_copy,
		.prepare = igo_nr_prepare,
		.reset = igo_nr_reset,
		.trigger = igo_nr_trigger,
	},
};

static SHARED_DATA struct comp_driver_info comp_igo_nr_info = {
	.drv = &comp_igo_nr,
};

UT_STATIC void sys_comp_igo_nr_init(void)
{
	comp_register(platform_shared_get(&comp_igo_nr_info,
					  sizeof(comp_igo_nr_info)));
}

DECLARE_MODULE(sys_comp_igo_nr_init);
