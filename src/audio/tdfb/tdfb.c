// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

/* Note: The script tools/tune/tdfb/example_all.sh can be used to re-calculate
 * all the beamformer topology data files if need. It also creates the additional
 * data files for simulated tests with testbench. Matlab or Octave is needed.
 */

#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/tdfb.h>
#include <user/trace.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/tdfb/tdfb_comp.h>
#include <sof/math/fir_generic.h>
#include <sof/math/fir_hifi2ep.h>
#include <sof/math/fir_hifi3.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* The driver assigns running numbers for control index. If there's single control of
 * type switch, enum, binary they all have index 0.
 */
#define CTRL_INDEX_PROCESS		0	/* switch */
#define CTRL_INDEX_DIRECTION		1	/* switch */
#define CTRL_INDEX_AZIMUTH		0	/* enum */
#define CTRL_INDEX_AZIMUTH_ESTIMATE	1	/* enum */
#define CTRL_INDEX_FILTERBANK		0	/* bytes */

static const struct comp_driver comp_tdfb;

LOG_MODULE_REGISTER(tdfb, CONFIG_SOF_LOG_LEVEL);

/* dd511749-d9fa-455c-b3a7-13585693f1af */
DECLARE_SOF_RT_UUID("tdfb", tdfb_uuid,  0xdd511749, 0xd9fa, 0x455c, 0xb3, 0xa7,
		    0x13, 0x58, 0x56, 0x93, 0xf1, 0xaf);

DECLARE_TR_CTX(tdfb_tr, SOF_UUID(tdfb_uuid), LOG_LEVEL_INFO);

/* IPC */

static int init_get_ctl_ipc(struct comp_dev *dev)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);
	int comp_id = dev_comp_id(dev);

	cd->ctrl_data = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, TDFB_GET_CTRL_DATA_SIZE);
	if (!cd->ctrl_data)
		return -ENOMEM;

	cd->ctrl_data->rhdr.hdr.cmd = SOF_IPC_GLB_COMP_MSG | SOF_IPC_COMP_GET_VALUE | comp_id;
	cd->ctrl_data->rhdr.hdr.size = TDFB_GET_CTRL_DATA_SIZE;
	cd->msg = ipc_msg_init(cd->ctrl_data->rhdr.hdr.cmd, cd->ctrl_data->rhdr.hdr.size);

	cd->ctrl_data->comp_id = comp_id;
	cd->ctrl_data->type = SOF_CTRL_TYPE_VALUE_CHAN_GET;
	cd->ctrl_data->cmd = SOF_CTRL_CMD_ENUM;
	cd->ctrl_data->index = CTRL_INDEX_AZIMUTH_ESTIMATE;
	cd->ctrl_data->num_elems = 0;
	return 0;
}

static void send_get_ctl_ipc(struct comp_dev *dev)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);

#if TDFB_ADD_DIRECTION_TO_GET_CMD
	cd->ctrl_data->chanv[0].channel = 0;
	cd->ctrl_data->chanv[0].value = cd->az_value_estimate;
	cd->ctrl_data->num_elems = 1;
#endif

	ipc_msg_send(cd->msg, cd->ctrl_data, false);
}

/*
 * The optimized FIR functions variants need to be updated into function
 * set_func.
 */

#if CONFIG_FORMAT_S16LE
static inline void set_s16_fir(struct tdfb_comp_data *cd)
{
	cd->tdfb_func = tdfb_fir_s16;
}
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
static inline void set_s24_fir(struct tdfb_comp_data *cd)
{
	cd->tdfb_func = tdfb_fir_s24;
}
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
static inline void set_s32_fir(struct tdfb_comp_data *cd)
{
	cd->tdfb_func = tdfb_fir_s32;
}
#endif /* CONFIG_FORMAT_S32LE */

static inline int set_func(struct comp_dev *dev, enum sof_ipc_frame fmt)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);

	switch (fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		comp_info(dev, "set_func(), SOF_IPC_FRAME_S16_LE");
		set_s16_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		comp_info(dev, "set_func(), SOF_IPC_FRAME_S24_4LE");
		set_s24_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		comp_info(dev, "set_func(), SOF_IPC_FRAME_S32_LE");
		set_s32_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(dev, "set_func(), invalid frame_fmt");
		return -EINVAL;
	}
	return 0;
}

/*
 * Control code functions next. The processing is in fir_ C modules.
 */

static void tdfb_free_delaylines(struct tdfb_comp_data *cd)
{
	struct fir_state_32x16 *fir = cd->fir;
	int i = 0;

	/* Free the common buffer for all EQs and point then
	 * each FIR channel delay line to NULL.
	 */
	rfree(cd->fir_delay);
	cd->fir_delay = NULL;
	cd->fir_delay_size = 0;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir[i].delay = NULL;
}

static int16_t *tdfb_filter_seek(struct sof_tdfb_config *config, int num_filters)
{
	struct sof_fir_coef_data *coef_data;
	int i;
	int16_t *coefp = ASSUME_ALIGNED(&config->data[0], 2); /* 2 for 16 bits data */

	/* Note: FIR coefficients are int16_t. An uint_8 type pointer coefp
	 * is used for jumping in the flexible array member structs coef_data.
	 */
	for (i = 0; i < num_filters; i++) {
		coef_data = (struct sof_fir_coef_data *)coefp;
		coefp = coef_data->coef + coef_data->length;
	}

	return coefp;
}

static int wrap_180(int a)
{
	if (a > 180)
		return ((a + 180) % 360) - 180;

	if (a < -180)
		return 180 - ((180 - a) % 360);

	return a;
}

static int tdfb_init_coef(struct tdfb_comp_data *cd, int source_nch,
			  int sink_nch)
{
	struct sof_fir_coef_data *coef_data;
	struct sof_tdfb_config *config = cd->config;
	int16_t *output_channel_mix_beam_off = NULL;
	int16_t *coefp;
	int size_sum = 0;
	int min_delta_idx; /* Index to beam angle with smallest delta vs. target */
	int min_delta; /* Smallest angle difference found in degrees */
	int max_ch;
	int num_filters;
	int target_az; /* Target azimuth angle in degrees */
	int delta; /* Target minus found angle in degrees absolute value */
	int idx;
	int s;
	int i;

	/* Sanity checks */
	if (config->num_output_channels > PLATFORM_MAX_CHANNELS ||
	    !config->num_output_channels) {
		comp_cl_err(&comp_tdfb, "tdfb_init_coef(), invalid num_output_channels %d",
			    config->num_output_channels);
		return -EINVAL;
	}

	if (config->num_output_channels != sink_nch) {
		comp_cl_err(&comp_tdfb, "tdfb_init_coef(), stream output channels count %d does not match configuration %d",
			    sink_nch, config->num_output_channels);
		return -EINVAL;
	}

	if (config->num_filters > SOF_TDFB_FIR_MAX_COUNT) {
		comp_cl_err(&comp_tdfb, "tdfb_init_coef(), invalid num_filters %d",
			    config->num_filters);
		return -EINVAL;
	}

	if (config->num_angles > SOF_TDFB_MAX_ANGLES) {
		comp_cl_err(&comp_tdfb, "tdfb_init_coef(), invalid num_angles %d",
			    config->num_angles);
		return -EINVAL;
	}

	if (config->beam_off_defined > 1) {
		comp_cl_err(&comp_tdfb, "tdfb_init_coef(), invalid beam_off_defined %d",
			    config->beam_off_defined);
		return -EINVAL;
	}

	if (config->num_mic_locations > SOF_TDFB_MAX_MICROPHONES) {
		comp_cl_err(&comp_tdfb, "tdfb_init_coef(), invalid num_mic_locations %d",
			    config->num_mic_locations);
		return -EINVAL;
	}

	/* In SOF v1.6 - 1.8 based beamformer topologies the multiple angles, mic locations,
	 * and beam on/off switch were not defined. Return error if such configuration is seen.
	 * A most basic blob has num_angles equals 1. Mic locations data is optional.
	 */
	if (config->num_angles == 0 && config->num_mic_locations == 0) {
		comp_cl_err(&comp_tdfb, "tdfb_init_coef(), ABI version less than 3.19.1 is not supported.");
		return -EINVAL;
	}

	/* Skip filter coefficients */
	num_filters = config->num_filters * (config->num_angles + config->beam_off_defined);
	coefp = tdfb_filter_seek(config, num_filters);

	/* Get shortcuts to input and output configuration */
	cd->input_channel_select = coefp;
	coefp += config->num_filters;
	cd->output_channel_mix = coefp;
	coefp += config->num_filters;
	cd->output_stream_mix = coefp;
	coefp += config->num_filters;

	/* Check if there's beam-off configured, then get pointers to beam angles data
	 * and microphone locations. Finally check that size matches.
	 */
	if (config->beam_off_defined) {
		output_channel_mix_beam_off = coefp;
		coefp += config->num_filters;
	}
	cd->filter_angles = (struct sof_tdfb_angle *)coefp;
	cd->mic_locations = (struct sof_tdfb_mic_location *)
		(&cd->filter_angles[config->num_angles]);
	if ((uint8_t *)&cd->mic_locations[config->num_mic_locations] !=
	    (uint8_t *)config + config->size) {
		comp_cl_err(&comp_tdfb, "tdfb_init_coef(), invalid config size");
		return -EINVAL;
	}

	/* Skip to requested coefficient set */
	min_delta = 360;
	min_delta_idx = 0;
	target_az = wrap_180(cd->az_value * config->angle_enum_mult + config->angle_enum_offs);

	for (i = 0; i < config->num_angles; i++) {
		delta = ABS(target_az - wrap_180(cd->filter_angles[i].azimuth));
		if (delta < min_delta) {
			min_delta = delta;
			min_delta_idx = i;
		}
	}

	idx = cd->filter_angles[min_delta_idx].filter_index;
	if (cd->beam_on) {
		comp_cl_info(&comp_tdfb, "tdfb_init_coef(), angle request %d, found %d, idx %d",
			     target_az, cd->filter_angles[min_delta_idx].azimuth, idx);
	} else if (config->beam_off_defined) {
		cd->output_channel_mix = output_channel_mix_beam_off;
		idx = config->num_filters * config->num_angles;
		comp_cl_info(&comp_tdfb, "tdfb_init_coef(), configure beam off");
	} else {
		comp_cl_info(&comp_tdfb, "tdfb_init_coef(), beam off is not defined, using filter %d, idx %d",
			     cd->filter_angles[min_delta_idx].azimuth, idx);
	}

	/* Seek to proper filter for requested angle or beam off configuration */
	coefp = tdfb_filter_seek(config, idx);

	/* Initialize filter bank */
	for (i = 0; i < config->num_filters; i++) {
		/* Get delay line size */
		coef_data = (struct sof_fir_coef_data *)coefp;
		s = fir_delay_size(coef_data);
		if (s > 0) {
			size_sum += s;
		} else {
			comp_cl_info(&comp_tdfb, "tdfb_init_coef(), FIR length %d is invalid",
				     coef_data->length);
			return -EINVAL;
		}

		/* Initialize coefficients for FIR filter and find next
		 * filter.
		 */
		fir_init_coef(&cd->fir[i], coef_data);
		coefp = coef_data->coef + coef_data->length;
	}

	/* Find max used input channel */
	max_ch = 0;
	for (i = 0; i < config->num_filters; i++) {
		if (cd->input_channel_select[i] > max_ch)
			max_ch = cd->input_channel_select[i];
	}

	/* The stream must contain at least the number of channels that is
	 * used for filters input.
	 */
	if (max_ch + 1 > source_nch) {
		comp_cl_err(&comp_tdfb, "tdfb_init_coef(), stream input channels count %d is not sufficient for configuration %d",
			    source_nch, max_ch + 1);
		return -EINVAL;
	}

	return size_sum;
}

static void tdfb_init_delay(struct tdfb_comp_data *cd)
{
	int32_t *fir_delay = cd->fir_delay;
	int i;

	/* Initialize second phase to set delay lines pointers */
	for (i = 0; i < cd->config->num_filters; i++) {
		if (cd->fir[i].length > 0)
			fir_init_delay(&cd->fir[i], &fir_delay);
	}
}

static int tdfb_setup(struct tdfb_comp_data *cd, int source_nch, int sink_nch)
{
	int delay_size;

	/* Set coefficients for each channel from coefficient blob */
	delay_size = tdfb_init_coef(cd, source_nch, sink_nch);
	if (delay_size < 0)
		return delay_size; /* Contains error code */

	/* If all channels were set to bypass there's no need to
	 * allocate delay. Just return with success.
	 */
	if (!delay_size)
		return 0;

	if (delay_size > cd->fir_delay_size) {
		/* Free existing FIR channels data if it was allocated */
		tdfb_free_delaylines(cd);

		/* Allocate all FIR channels data in a big chunk and clear it */
		cd->fir_delay = rballoc(0, SOF_MEM_CAPS_RAM, delay_size);
		if (!cd->fir_delay) {
			comp_cl_err(&comp_tdfb, "tdfb_setup(), delay allocation failed for size %d",
				    delay_size);
			return -ENOMEM;
		}

		memset(cd->fir_delay, 0, delay_size);
		cd->fir_delay_size = delay_size;
	}

	/* Assign delay line to all channel filters */
	tdfb_init_delay(cd);

	return 0;
}

/*
 * End of algorithm code. Next the standard component methods.
 */

static struct comp_dev *tdfb_new(const struct comp_driver *drv,
				 struct comp_ipc_config *config,
				 void *spec)
{
	struct ipc_config_process *ipc_tdfb = spec;
	struct comp_dev *dev = NULL;
	struct tdfb_comp_data *cd = NULL;
	size_t bs = ipc_tdfb->size;
	int ret;
	int i;

	comp_cl_info(&comp_tdfb, "tdfb_new()");

	/* Check first that configuration blob size is sane */
	if (bs > SOF_TDFB_MAX_SIZE) {
		comp_cl_err(&comp_tdfb, "tdfb_new() error: configuration blob size = %u > %d",
			    bs, SOF_TDFB_MAX_SIZE);
		return NULL;
	}

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		goto fail;

	comp_set_drvdata(dev, cd);

	/* Defaults for processing function pointer tdfb_func, fir_delay
	 * pointer, are NULL. Fir_delay_size is zero from rzalloc().
	 */

	/* Defaults for enum controls are zeros from rzalloc()
	 * az_value is zero, beam off is false, and update is false.
	 */

	/* Initialize IPC for direction of arrival estimate update */
	ret = init_get_ctl_ipc(dev);
	if (ret)
		goto cd_fail;

	/* Handler for configuration data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_cl_err(&comp_tdfb, "tdfb_new(): comp_data_blob_handler_new() failed.");
		goto cd_fail;
	}

	/* Get configuration data and reset FIR filters */
	ret = comp_init_data_blob(cd->model_handler, bs, ipc_tdfb->data);
	if (ret < 0) {
		comp_cl_err(&comp_tdfb, "tdfb_new(): comp_init_data_blob() failed.");
		goto cd_fail;
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	dev->state = COMP_STATE_READY;
	return dev;

cd_fail:
	comp_data_blob_handler_free(cd->model_handler); /* works for non-initialized also */
	rfree(cd);
fail:
	rfree(dev);
	return NULL;
}

static void tdfb_free(struct comp_dev *dev)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "tdfb_free()");

	ipc_msg_free(cd->msg);
	tdfb_free_delaylines(cd);
	comp_data_blob_handler_free(cd->model_handler);
	tdfb_direction_free(cd);
	rfree(cd->ctrl_data);
	rfree(cd);
	rfree(dev);
}

static int tdfb_cmd_get_data(struct comp_dev *dev,
			     struct sof_ipc_ctrl_data *cdata, int max_size)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);

	if (cdata->cmd == SOF_CTRL_CMD_BINARY) {
		comp_dbg(dev, "tdfb_cmd_get_data(), SOF_CTRL_CMD_BINARY");
		return comp_data_blob_get_cmd(cd->model_handler, cdata, max_size);
	}

	comp_err(dev, "tdfb_cmd_get_data() error: invalid cdata->cmd");
	return -EINVAL;
}

static int tdfb_cmd_switch_get(struct sof_ipc_ctrl_data *cdata, struct tdfb_comp_data *cd)
{
	int j;

	/* Fail if wrong index in control, needed if several in same type */
	if (cdata->index != CTRL_INDEX_PROCESS)
		return -EINVAL;

	for (j = 0; j < cdata->num_elems; j++)
		cdata->chanv[j].value = cd->beam_on;

	return 0;
}

static int tdfb_cmd_enum_get(struct sof_ipc_ctrl_data *cdata, struct tdfb_comp_data *cd)
{
	int j;

	switch (cdata->index) {
	case CTRL_INDEX_AZIMUTH:
		for (j = 0; j < cdata->num_elems; j++)
			cdata->chanv[j].value = cd->az_value;

		break;
	case CTRL_INDEX_AZIMUTH_ESTIMATE:
		for (j = 0; j < cdata->num_elems; j++)
			cdata->chanv[j].value = cd->az_value_estimate;

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tdfb_cmd_get_value(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_dbg(dev, "tdfb_cmd_get_value(), SOF_CTRL_CMD_ENUM index=%d", cdata->index);
		return tdfb_cmd_enum_get(cdata, cd);
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "tdfb_cmd_get_value(), SOF_CTRL_CMD_SWITCH index=%d", cdata->index);
		return tdfb_cmd_switch_get(cdata, cd);
	}

	comp_err(dev, "tdfb_cmd_get_value() error: invalid cdata->cmd");
	return -EINVAL;
}

static int tdfb_cmd_set_data(struct comp_dev *dev,
			     struct sof_ipc_ctrl_data *cdata)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);

	if (cdata->cmd == SOF_CTRL_CMD_BINARY) {
		comp_dbg(dev, "tdfb_cmd_set_data(), SOF_CTRL_CMD_BINARY");
		return comp_data_blob_set_cmd(cd->model_handler, cdata);
	}

	comp_err(dev, "tdfb_cmd_set_data() error: invalid cdata->cmd");
	return -EINVAL;
}

static int tdfb_cmd_enum_set(struct sof_ipc_ctrl_data *cdata, struct tdfb_comp_data *cd)
{
	if (cdata->num_elems != 1)
		return -EINVAL;

	if (cdata->chanv[0].value > SOF_TDFB_MAX_ANGLES)
		return -EINVAL;

	switch (cdata->index) {
	case CTRL_INDEX_AZIMUTH:
		cd->az_value = cdata->chanv[0].value;
		cd->update = true;
		break;
	case CTRL_INDEX_AZIMUTH_ESTIMATE:
		cd->az_value_estimate = cdata->chanv[0].value;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tdfb_cmd_switch_set(struct sof_ipc_ctrl_data *cdata, struct tdfb_comp_data *cd)
{
	if (cdata->num_elems != 1)
		return -EINVAL;

	switch (cdata->index) {
	case CTRL_INDEX_PROCESS:
		cd->beam_on = cdata->chanv[0].value;
		cd->update = true;
		break;
	case CTRL_INDEX_DIRECTION:
		cd->direction_updates = cdata->chanv[0].value;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tdfb_cmd_set_value(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_dbg(dev, "tdfb_cmd_set_value(), SOF_CTRL_CMD_ENUM index=%d", cdata->index);
		return tdfb_cmd_enum_set(cdata, cd);
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "tdfb_cmd_set_value(), SOF_CTRL_CMD_SWITCH index=%d", cdata->index);
		return tdfb_cmd_switch_set(cdata, cd);
	}

	comp_err(dev, "tdfb_cmd_set_value() error: invalid cdata->cmd");
	return -EINVAL;
}

/* used to pass standard and bespoke commands (with data) to component */
static int tdfb_cmd(struct comp_dev *dev, int cmd, void *data,
		    int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);

	comp_info(dev, "tdfb_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		comp_dbg(dev, "tdfb_cmd(): COMP_CMD_SET_DATA");
		return tdfb_cmd_set_data(dev, cdata);
	case COMP_CMD_GET_DATA:
		comp_dbg(dev, "tdfb_cmd(): COMP_CMD_GET_DATA");
		return tdfb_cmd_get_data(dev, cdata, max_data_size);
	case COMP_CMD_SET_VALUE:
		comp_dbg(dev, "tdfb_cmd(): COMP_CMD_SET_VALUE");
		return tdfb_cmd_set_value(dev, cdata);
	case COMP_CMD_GET_VALUE:
		comp_dbg(dev, "tdfb_cmd(): COMP_CMD_GET_VALUE");
		return tdfb_cmd_get_value(dev, cdata);
	}

	comp_err(dev, "tdfb_cmd() error: invalid command");
	return -EINVAL;
}

static void tdfb_process(struct comp_dev *dev, struct comp_buffer __sparse_cache *source,
			 struct comp_buffer __sparse_cache *sink, int frames,
			 uint32_t source_bytes, uint32_t sink_bytes)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);

	buffer_stream_invalidate(source, source_bytes);

	cd->tdfb_func(cd, &source->stream, &sink->stream, frames);

	buffer_stream_writeback(sink, sink_bytes);

	/* calc new free and available */
	comp_update_buffer_consume(source, source_bytes);
	comp_update_buffer_produce(sink, sink_bytes);

	/* Update sound direction estimate */
	tdfb_direction_estimate(cd, frames, source->stream.channels);
	comp_dbg(dev, "tdfb_dint %u %d %d %d", cd->direction.trigger, cd->direction.level,
		 (int32_t)(cd->direction.level_ambient >> 32), cd->direction.az_slow);
}

/* copy and process stream data from source to sink buffers */
static int tdfb_copy(struct comp_dev *dev)
{
	struct comp_copy_limits cl;
	struct comp_buffer *sourceb, *sinkb;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;
	int n;

	comp_dbg(dev, "tdfb_copy()");

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	source_c = buffer_acquire(sourceb);
	sink_c = buffer_acquire(sinkb);

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = tdfb_setup(cd, source_c->stream.channels, sink_c->stream.channels);
		if (ret < 0) {
			comp_err(dev, "tdfb_copy(), failed FIR setup");
			goto out;
		}
	}

	/* Handle enum controls */
	if (cd->update) {
		cd->update = false;
		ret = tdfb_setup(cd, source_c->stream.channels, sink_c->stream.channels);
		if (ret < 0) {
			comp_err(dev, "tdfb_copy(), failed FIR setup");
			goto out;
		}
	}

	/* Get source, sink, number of frames etc. to process. */
	comp_get_copy_limits(source_c, sink_c, &cl);

	/*
	 * Process only even number of frames with the FIR function. The
	 * optimized filter function loads the successive input samples from
	 * internal delay line with a 64 bit load operation.
	 */
	cl.frames = MIN(cl.frames, cd->max_frames);
	if (cl.frames >= 2) {
		n = (cl.frames >> 1) << 1;

		/* Run the process function */
		tdfb_process(dev, source_c, sink_c, n,
			     n * cl.source_frame_bytes,
			     n * cl.sink_frame_bytes);
	}

	if (cd->direction_updates && cd->direction_change) {
		send_get_ctl_ipc(dev);
		cd->direction_change = false;
		comp_dbg(dev, "tdfb_dupd %d %d", cd->az_value_estimate, cd->direction.az_slow);
	}

out:

	buffer_release(sink_c);
	buffer_release(source_c);

	return ret;
}

static int tdfb_prepare(struct comp_dev *dev)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb, *sinkb;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	int ret;

	comp_info(dev, "tdfb_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* Find source and sink buffers */
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

	source_c = buffer_acquire(sourceb);
	sink_c = buffer_acquire(sinkb);

	/* Initialize filter */
	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
	if (!cd->config) {
		ret = -EINVAL;
		goto out;
	}

	ret = tdfb_setup(cd, sourceb->stream.channels, sinkb->stream.channels);
	if (ret < 0) {
		comp_err(dev, "tdfb_prepare() error: tdfb_setup failed.");
		goto out;
	}

	/* Clear in/out buffers */
	memset(cd->in, 0, TDFB_IN_BUF_LENGTH * sizeof(int32_t));
	memset(cd->out, 0, TDFB_IN_BUF_LENGTH * sizeof(int32_t));

	ret = set_func(dev, source_c->stream.frame_fmt);
	if (ret)
		goto out;

	/* The max. amount of processing need to be limited for sound direction
	 * processing. Max frames is used in tdfb_direction_init() and copy().
	 */
	cd->max_frames = Q_MULTSR_16X16((int32_t)dev->frames, TDFB_MAX_FRAMES_MULT_Q14, 0, 14, 0);
	comp_info(dev, "dev_frames = %d, max_frames = %d", dev->frames, cd->max_frames);

	/* Initialize tracking */
	ret = tdfb_direction_init(cd, sourceb->stream.rate, sourceb->stream.channels);
	if (!ret) {
		comp_info(dev, "max_lag = %d, xcorr_size = %d",
			  cd->direction.max_lag, cd->direction.d_size);
		comp_info(dev, "line_array = %d, a_step = %d, a_offs = %d",
			  (int)cd->direction.line_array, cd->config->angle_enum_mult,
			  cd->config->angle_enum_offs);
	}

out:
	if (ret < 0)
		comp_set_state(dev, COMP_TRIGGER_RESET);

	buffer_release(sink_c);
	buffer_release(source_c);

	return ret;
}

/* set component audio stream parameters */
static int tdfb_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	int err;

	comp_info(dev, "tdfb_params()");

	err = comp_verify_params(dev, BUFF_PARAMS_CHANNELS, params);
	if (err < 0) {
		comp_err(dev, "tdfb_params(): pcm params verification failed.");
		return -EINVAL;
	}

	return 0;
}

static int tdfb_reset(struct comp_dev *dev)
{
	int i;
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "tdfb_reset()");

	tdfb_free_delaylines(cd);

	cd->tdfb_func = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	/* Clear in/out buffers */
	memset(cd->in, 0, TDFB_IN_BUF_LENGTH * sizeof(int32_t));
	memset(cd->out, 0, TDFB_IN_BUF_LENGTH * sizeof(int32_t));

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static int tdfb_trigger(struct comp_dev *dev, int cmd)
{
	int ret = 0;

	comp_info(dev, "tdfb_trigger(), command = %u", cmd);

	ret = comp_set_state(dev, cmd);
	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		ret = PPL_STATUS_PATH_STOP;

	return ret;
}

static const struct comp_driver comp_tdfb = {
	.uid = SOF_RT_UUID(tdfb_uuid),
	.tctx	= &tdfb_tr,
	.ops = {
		.create = tdfb_new,
		.free = tdfb_free,
		.params = tdfb_params,
		.cmd = tdfb_cmd,
		.copy = tdfb_copy,
		.prepare = tdfb_prepare,
		.reset = tdfb_reset,
		.trigger = tdfb_trigger,
	},
};

static SHARED_DATA struct comp_driver_info comp_tdfb_info = {
	.drv = &comp_tdfb,
};

UT_STATIC void sys_comp_tdfb_init(void)
{
	comp_register(platform_shared_get(&comp_tdfb_info,
					  sizeof(comp_tdfb_info)));
}

DECLARE_MODULE(sys_comp_tdfb_init);
