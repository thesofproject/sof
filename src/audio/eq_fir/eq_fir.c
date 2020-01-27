// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/eq_fir/fir_config.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <kernel/abi.h>
#include <user/eq.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#if FIR_GENERIC
#include <sof/audio/eq_fir/fir.h>
#endif

#if FIR_HIFIEP
#include <sof/audio/eq_fir/fir_hifi2ep.h>
#endif

#if FIR_HIFI3
#include <sof/audio/eq_fir/fir_hifi3.h>
#endif

/* tracing */
#define trace_eq(__e, ...) trace_event(TRACE_CLASS_EQ_FIR, __e, ##__VA_ARGS__)
#define trace_eq_with_ids(comp_ptr, __e, ...)			\
	trace_event_comp(TRACE_CLASS_EQ_FIR, comp_ptr,		\
			 __e, ##__VA_ARGS__)

#define tracev_eq(__e, ...) tracev_event(TRACE_CLASS_EQ_FIR, __e, ##__VA_ARGS__)
#define tracev_eq_with_ids(comp_ptr, __e, ...)			\
	tracev_event_comp(TRACE_CLASS_EQ_FIR, comp_ptr,		\
			  __e, ##__VA_ARGS__)

#define trace_eq_error(__e, ...) \
	trace_error(TRACE_CLASS_EQ_FIR, __e, ##__VA_ARGS__)
#define trace_eq_error_with_ids(comp_ptr, __e, ...)		\
	trace_error_comp(TRACE_CLASS_EQ_FIR, comp_ptr,		\
			 __e, ##__VA_ARGS__)

/* src component private data */
struct comp_data {
	struct fir_state_32x16 fir[PLATFORM_MAX_CHANNELS]; /**< filters state */
	struct sof_eq_fir_config *config;	/**< pointer to setup blob */
	struct sof_eq_fir_config *config_new;	/**< pointer to new setup */
	enum sof_ipc_frame source_format;	/**< source frame format */
	enum sof_ipc_frame sink_format;		/**< sink frame format */
	int32_t *fir_delay;			/**< pointer to allocated RAM */
	size_t fir_delay_size;			/**< allocated size */
	bool config_ready;			/**< set when fully received */
	void (*eq_fir_func)(struct fir_state_32x16 fir[],
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    int frames, int nch);
};

/*
 * The optimized FIR functions variants need to be updated into function
 * set_fir_func.
 */

#if FIR_HIFI3
#if CONFIG_FORMAT_S16LE
static inline void set_s16_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s16_hifi3;
}
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
static inline void set_s24_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s24_hifi3;
}
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
static inline void set_s32_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s32_hifi3;
}
#endif /* CONFIG_FORMAT_S32LE */

#elif FIR_HIFIEP
#if CONFIG_FORMAT_S16LE
static inline void set_s16_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s16_hifiep;
}
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
static inline void set_s24_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s24_hifiep;
}
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
static inline void set_s32_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s32_hifiep;
}
#endif /* CONFIG_FORMAT_S32LE */
#else
/* FIR_GENERIC */
#if CONFIG_FORMAT_S16LE
static inline void set_s16_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_s16;
}
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
static inline void set_s24_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_s24;
}
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
static inline void set_s32_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_s32;
}
#endif /* CONFIG_FORMAT_S32LE */
#endif

static inline int set_fir_func(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	switch (sourceb->stream.frame_fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		trace_eq_with_ids(dev, "set_fir_func(), SOF_IPC_FRAME_S16_LE");
		set_s16_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		trace_eq_with_ids(dev, "set_fir_func(), SOF_IPC_FRAME_S24_4LE");
		set_s24_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		trace_eq_with_ids(dev, "set_fir_func(), SOF_IPC_FRAME_S32_LE");
		set_s32_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		trace_eq_error_with_ids(dev, "set_fir_func(), invalid frame_fmt");
		return -EINVAL;
	}
	return 0;
}

/* Pass-trough functions to replace FIR core while not configured for
 * response.
 */

#if CONFIG_FORMAT_S16LE
static void eq_fir_s16_passthrough(struct fir_state_32x16 fir[],
				   const struct audio_stream *source,
				   struct audio_stream *sink,
				   int frames, int nch)
{
	audio_stream_copy_s16(source, sink, frames * nch);
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
static void eq_fir_s32_passthrough(struct fir_state_32x16 fir[],
				   const struct audio_stream *source,
				   struct audio_stream *sink,
				   int frames, int nch)
{
	audio_stream_copy_s32(source, sink, frames * nch);
}
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */

/* Function to select pass-trough depending on PCM format */

static inline int set_pass_func(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	switch (sourceb->stream.frame_fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		trace_eq_with_ids(dev, "set_pass_func(), SOF_IPC_FRAME_S16_LE");
		cd->eq_fir_func = eq_fir_s16_passthrough;
		break;
#endif /* CONFIG_FORMAT_S32LE */
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		trace_eq_with_ids(dev, "set_pass_func(), SOF_IPC_FRAME_S32_LE");
		cd->eq_fir_func = eq_fir_s32_passthrough;
		break;
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */
	default:
		trace_eq_error_with_ids(dev, "set_pass_func() error: "
					"invalid dev->params.frame_fmt");
		return -EINVAL;
	}
	return 0;
}

/*
 * EQ control code is next. The processing is in fir_ C modules.
 */

static void eq_fir_free_parameters(struct sof_eq_fir_config **config)
{
	rfree(*config);
	*config = NULL;
}

static void eq_fir_free_delaylines(struct comp_data *cd)
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

static int eq_fir_init_coef(struct sof_eq_fir_config *config,
			    struct fir_state_32x16 *fir, int nch)
{
	struct sof_eq_fir_coef_data *lookup[SOF_EQ_FIR_MAX_RESPONSES];
	struct sof_eq_fir_coef_data *eq;
	int16_t *assign_response;
	int16_t *coef_data;
	size_t size_sum = 0;
	int resp = 0;
	int i;
	int j;
	int s;

	trace_eq("eq_fir_init_coef(), response assign for %u channels, %u"
		 " responses", config->channels_in_config,
		 config->number_of_responses);

	/* Sanity checks */
	if (nch > PLATFORM_MAX_CHANNELS ||
	    config->channels_in_config > PLATFORM_MAX_CHANNELS ||
	    !config->channels_in_config) {
		trace_eq_error("eq_fir_init_coef(), invalid channels count");
		return -EINVAL;
	}
	if (config->number_of_responses > SOF_EQ_FIR_MAX_RESPONSES) {
		trace_eq_error("eq_fir_init_coef(), # of resp exceeds max");
		return -EINVAL;
	}

	/* Collect index of respose start positions in all_coefficients[]  */
	j = 0;
	assign_response = ASSUME_ALIGNED(&config->data[0], 4);
	coef_data = ASSUME_ALIGNED(&config->data[config->channels_in_config],
				   4);
	for (i = 0; i < SOF_EQ_FIR_MAX_RESPONSES; i++) {
		if (i < config->number_of_responses) {
			eq = (struct sof_eq_fir_coef_data *)&coef_data[j];
			lookup[i] = eq;
			j += SOF_EQ_FIR_COEF_NHEADER + coef_data[j];
		} else {
			lookup[i] = NULL;
		}
	}

	/* Initialize 1st phase */
	for (i = 0; i < nch; i++) {
		/* Check for not reading past blob response to channel assign
		 * map. The previous channel response is assigned for any
		 * additional channels in the stream. It allows to use single
		 * channel configuration to setup multi channel equalization
		 * with the same response.
		 */
		if (i < config->channels_in_config)
			resp = assign_response[i];

		if (resp < 0) {
			/* Initialize EQ channel to bypass and continue with
			 * next channel response.
			 */
			trace_eq("eq_fir_init_coef(), ch %d is set to bypass",
				 i);
			fir_reset(&fir[i]);
			continue;
		}

		if (resp >= config->number_of_responses) {
			trace_eq_error("eq_fir_init_coef(), requested response %d"
				 " exceeds what has been defined", resp);
			return -EINVAL;
		}

		/* Initialize EQ coefficients. */
		eq = lookup[resp];
		s = fir_delay_size(eq);
		if (s > 0) {
			size_sum += s;
		} else {
			trace_eq("eq_fir_init_coef(), FIR length %d"
				 " is invalid", eq->length);
			return -EINVAL;
		}

#if defined FIR_MAX_LENGTH_BUILD_SPECIFIC
		if (fir[i].taps * nch > FIR_MAX_LENGTH_BUILD_SPECIFIC) {
			trace_eq_error("Filter length %d"
					" exceeds limitation for build.",
					fir[i].length);
			return -EINVAL;
		}
#endif

		fir_init_coef(&fir[i], eq);
		trace_eq("eq_fir_init_coef(), ch %d is set to response = %d",
			 i, resp);
	}

	return size_sum;
}

static void eq_fir_init_delay(struct fir_state_32x16 *fir,
			      int32_t *delay_start, int nch)
{
	int32_t *fir_delay = delay_start;
	int i;

	/* Initialize 2nd phase to set EQ delay lines pointers */
	for (i = 0; i < nch; i++) {
		if (fir[i].length > 0)
			fir_init_delay(&fir[i], &fir_delay);
	}
}

static int eq_fir_setup(struct comp_data *cd, int nch)
{
	int delay_size;

	/* Free existing FIR channels data if it was allocated */
	eq_fir_free_delaylines(cd);

	/* Set coefficients for each channel EQ from coefficient blob */
	delay_size = eq_fir_init_coef(cd->config, cd->fir, nch);
	if (delay_size < 0)
		return delay_size; /* Contains error code */

	/* If all channels were set to bypass there's no need to
	 * allocate delay. Just return with success.
	 */
	if (!delay_size)
		return 0;

	/* Allocate all FIR channels data in a big chunk and clear it */
	cd->fir_delay = rballoc(0, SOF_MEM_CAPS_RAM, delay_size);
	if (!cd->fir_delay) {
		trace_eq_error("eq_fir_setup(), delay allocation failed for size %d",
			       delay_size);
		return -ENOMEM;
	}

	memset(cd->fir_delay, 0, delay_size);
	cd->fir_delay_size = delay_size;

	/* Assign delay line to each channel EQ */
	eq_fir_init_delay(cd->fir, cd->fir_delay, nch);
	return 0;
}

/*
 * End of algorithm code. Next the standard component methods.
 */

static struct comp_dev *eq_fir_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct comp_data *cd;
	struct sof_ipc_comp_process *fir;
	struct sof_ipc_comp_process *ipc_fir
		= (struct sof_ipc_comp_process *)comp;
	size_t bs = ipc_fir->size;
	int i;
	int ret;

	trace_eq("eq_fir_new()");

	if (IPC_IS_SIZE_INVALID(ipc_fir->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_EQ_FIR, ipc_fir->config);
		return NULL;
	}

	/* Check first before proceeding with dev and cd that coefficients
	 * blob size is sane.
	 */
	if (bs > SOF_EQ_FIR_MAX_SIZE) {
		trace_eq_error("eq_fir_new() error: coefficients blob size = "
			       "%u > SOF_EQ_FIR_MAX_SIZE", bs);
		return NULL;
	}

	dev = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	fir = (struct sof_ipc_comp_process *)&dev->comp;
	ret = memcpy_s(fir, sizeof(*fir), ipc_fir,
		       sizeof(struct sof_ipc_comp_process));
	assert(!ret);

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->eq_fir_func = NULL;
	cd->config = NULL;
	cd->config_new = NULL;
	cd->config_ready = false;
	cd->fir_delay = NULL;
	cd->fir_delay_size = 0;

	/* Allocate and make a copy of the coefficients blob and reset FIR. If
	 * the EQ is configured later in run-time the size is zero.
	 */
	if (bs) {
		cd->config = rballoc(0, SOF_MEM_CAPS_RAM, bs);
		if (!cd->config) {
			rfree(dev);
			rfree(cd);
			return NULL;
		}

		ret = memcpy_s(cd->config, bs, ipc_fir->data, bs);
		assert(!ret);
		cd->config_ready = true;
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void eq_fir_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_eq_with_ids(dev, "eq_fir_free()");

	eq_fir_free_delaylines(cd);
	eq_fir_free_parameters(&cd->config);
	eq_fir_free_parameters(&cd->config_new);

	rfree(cd);
	rfree(dev);
}

/* set component audio stream parameters */
static int eq_fir_params(struct comp_dev *dev,
			 struct sof_ipc_stream_params *params)
{
	trace_eq_with_ids(dev, "eq_fir_params()");

	/* All configuration work is postponed to prepare(). */
	return 0;
}

static int fir_cmd_get_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata, int max_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	unsigned char *dst, *src;
	size_t offset;
	size_t bs;
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		trace_eq_with_ids(dev, "fir_cmd_get_data(), SOF_CTRL_CMD_BINARY");

		max_size -= sizeof(struct sof_ipc_ctrl_data) +
			sizeof(struct sof_abi_hdr);

		/* Copy back to user space */
		if (cd->config) {
			src = (unsigned char *)cd->config;
			dst = (unsigned char *)cdata->data->data;
			bs = cd->config->size;
			cdata->elems_remaining = 0;
			offset = 0;
			if (bs > max_size) {
				bs = (cdata->msg_index + 1) * max_size > bs ?
					bs - cdata->msg_index * max_size :
					max_size;
				offset = cdata->msg_index * max_size;
				cdata->elems_remaining = cd->config->size -
					offset;
			}
			cdata->num_elems = bs;
			trace_eq_with_ids(dev, "fir_cmd_get_data(), "
					  "blob size %zu "
					  "msg index %u "
					  "max size %u "
					  "offset %zu",
					   bs, cdata->msg_index,
					   max_size, offset);
			ret = memcpy_s(dst, ((struct sof_abi_hdr *)
				       (cdata->data))->size, src + offset,
				       bs);
			assert(!ret);

			cdata->data->abi = SOF_ABI_VERSION;
			cdata->data->size = bs;
		} else {
			trace_eq_error_with_ids(dev, "fir_cmd_get_data() error:"
						"invalid cd->config");
			ret = -EINVAL;
		}
		break;
	default:
		trace_eq_error_with_ids(dev, "fir_cmd_get_data() error: "
					"invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int fir_cmd_set_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	unsigned char *dst, *src;
	uint32_t offset;
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		trace_eq_with_ids(dev, "fir_cmd_set_data(), SOF_CTRL_CMD_BINARY");

		/* Check that there is no work-in-progress previous request */
		if (cd->config_new && cdata->msg_index == 0) {
			trace_eq_error_with_ids(dev, "fir_cmd_set_data(), busy with previous request");
			return -EBUSY;
		}

		/* Copy new config, find size from header */
		trace_eq_with_ids(dev, "fir_cmd_set_data(): "
				  "blob size: %u msg_index %u",
				  cdata->num_elems + cdata->elems_remaining,
				  cdata->msg_index);
		if (cdata->num_elems + cdata->elems_remaining >
		    SOF_EQ_FIR_MAX_SIZE)
			return -EINVAL;

		if (cdata->msg_index == 0) {
			/* Allocate buffer for copy of the blob. */
			cd->config_new = rballoc(0, SOF_MEM_CAPS_RAM,
						 cdata->num_elems +
						 cdata->elems_remaining);

			if (!cd->config_new) {
				trace_eq_error_with_ids(dev, "fir_cmd_set_data() "
							"error: buffer "
							"allocation failed");
				return -EINVAL;
			}

			cd->config_ready = false;
			offset = 0;
		} else {
			assert(cd->config_new);
			offset = cd->config_new->size - cdata->elems_remaining -
				cdata->num_elems;
		}

		dst = (unsigned char *)cd->config_new;
		src = (unsigned char *)cdata->data->data;

		/* Just copy the configuration. The EQ will be initialized in
		 * prepare().
		 */
		ret = memcpy_s(dst + offset, cdata->num_elems +
			       cdata->elems_remaining - offset, src,
			       cdata->num_elems);
		assert(!ret);

		/* we can check data when elems_remaining == 0 */
		if (cdata->elems_remaining == 0) {
			/* The new configuration is OK to be applied */
			cd->config_ready = true;

			/* If component state is READY we can omit old
			 * configuration immediately. When in playback/capture
			 * the new configuration presence is checked in copy().
			 */
			if (dev->state ==  COMP_STATE_READY)
				eq_fir_free_parameters(&cd->config);

			/* If there is no existing configuration the received
			 * can be set to current immediately. It will be
			 * applied in prepare() when streaming starts.
			 */
			if (!cd->config) {
				cd->config = cd->config_new;
				cd->config_new = NULL;
			}
		}
		break;
	default:
		trace_eq_error_with_ids(dev, "fir_cmd_set_data() "
					"error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int eq_fir_cmd(struct comp_dev *dev, int cmd, void *data,
		      int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	trace_eq_with_ids(dev, "eq_fir_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = fir_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = fir_cmd_get_data(dev, cdata, max_data_size);
		break;
	default:
		trace_eq_error_with_ids(dev, "eq_fir_cmd() error: "
					"invalid command");
		ret = -EINVAL;
	}

	return ret;
}

static int eq_fir_trigger(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	trace_eq_with_ids(dev, "eq_fir_trigger()");

	if (cmd == COMP_TRIGGER_START || cmd == COMP_TRIGGER_RELEASE)
		assert(cd->eq_fir_func);

	return comp_set_state(dev, cmd);
}

/* copy and process stream data from source to sink buffers */
static int eq_fir_copy(struct comp_dev *dev)
{
	struct comp_copy_limits cl;
	struct comp_buffer *sourceb;
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;
	int n;

	tracev_eq_with_ids(dev, "eq_fir_copy()");

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	/* Check for changed configuration */
	if (cd->config_new && cd->config_ready) {
		eq_fir_free_parameters(&cd->config);
		cd->config = cd->config_new;
		cd->config_new = NULL;
		ret = eq_fir_setup(cd, sourceb->stream.channels);
		if (ret < 0) {
			trace_eq_error_with_ids(dev, "eq_fir_copy(), failed FIR setup");
			return ret;
		}
	}

	/* Get source, sink, number of frames etc. to process. */
	comp_get_copy_limits(dev, &cl);

	/*
	 * Process only even number of frames with the FIR function. The
	 * optimized filter function loads the successive input samples from
	 * internal delay line with a 64 bit load operation. The other odd
	 * (or any) number of frames capable FIR version would permanently
	 * break the delay line alignment if called with odd number of frames
	 * so it can't be used here.
	 */
	if (cl.frames >= 2) {
		n = (cl.frames >> 1) << 1;

		/* Run EQ function */
		cd->eq_fir_func(cd->fir, &cl.source->stream, &cl.sink->stream,
				n, cl.source->stream.channels);

		/* calc new free and available */
		comp_update_buffer_consume(cl.source,
					   n * cl.source_frame_bytes);
		comp_update_buffer_produce(cl.sink, n * cl.sink_frame_bytes);
	}

	return 0;
}

static int eq_fir_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	uint32_t sink_period_bytes;
	int ret;

	trace_eq_with_ids(dev, "eq_fir_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* EQ component will only ever have 1 source and 1 sink buffer. */
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

	/* get source data format */
	cd->source_format = sourceb->stream.frame_fmt;

	/* get sink data format and period bytes */
	cd->sink_format = sinkb->stream.frame_fmt;
	sink_period_bytes = audio_stream_period_bytes(&sinkb->stream,
						      dev->frames);

	/* Rewrite params format for this component to match the host side. */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		sourceb->stream.frame_fmt = cd->source_format;
	else
		sinkb->stream.frame_fmt = cd->sink_format;

	if (sinkb->stream.size < config->periods_sink * sink_period_bytes) {
		trace_eq_error_with_ids(dev, "eq_fir_prepare() error: "
					"sink buffer size is insufficient");
		ret = -ENOMEM;
		goto err;
	}

	/* Initialize EQ */
	if (cd->config && cd->config_ready) {
		ret = eq_fir_setup(cd, sourceb->stream.channels);
		if (ret < 0) {
			trace_eq_error_with_ids(dev, "eq_fir_prepare() error: "
						"eq_fir_setup failed.");
			goto err;
		}

		ret = set_fir_func(dev);
		return ret;
	}

	ret = set_pass_func(dev);
	return ret;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int eq_fir_reset(struct comp_dev *dev)
{
	int i;
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_eq_with_ids(dev, "eq_fir_reset()");

	eq_fir_free_delaylines(cd);

	cd->eq_fir_func = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static void eq_fir_cache(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd;

	switch (cmd) {
	case CACHE_WRITEBACK_INV:
		trace_eq_with_ids(dev, "eq_fir_cache(), CACHE_WRITEBACK_INV");

		cd = comp_get_drvdata(dev);
		if (cd->config)
			dcache_writeback_invalidate_region(cd->config,
							   cd->config->size);
		if (cd->fir_delay)
			dcache_writeback_invalidate_region(cd->fir_delay,
							   cd->fir_delay_size);

		dcache_writeback_invalidate_region(cd, sizeof(*cd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case CACHE_INVALIDATE:
		trace_eq_with_ids(dev, "eq_fir_cache(), CACHE_INVALIDATE");

		dcache_invalidate_region(dev, sizeof(*dev));

		/* Note: The component data need to be retrieved after
		 * the dev data has been invalidated.
		 */
		cd = comp_get_drvdata(dev);
		dcache_invalidate_region(cd, sizeof(*cd));

		if (cd->fir_delay)
			dcache_invalidate_region(cd->fir_delay,
						 cd->fir_delay_size);
		if (cd->config)
			dcache_invalidate_region(cd->config,
						 cd->config->size);

		break;
	}
}

static const struct comp_driver comp_eq_fir = {
	.type = SOF_COMP_EQ_FIR,
	.ops = {
		.new = eq_fir_new,
		.free = eq_fir_free,
		.params = eq_fir_params,
		.cmd = eq_fir_cmd,
		.trigger = eq_fir_trigger,
		.copy = eq_fir_copy,
		.prepare = eq_fir_prepare,
		.reset = eq_fir_reset,
		.cache = eq_fir_cache,
	},
};

static SHARED_DATA struct comp_driver_info comp_eq_fir_info = {
	.drv = &comp_eq_fir,
};

static void sys_comp_eq_fir_init(void)
{
	comp_register(platform_shared_get(&comp_eq_fir_info,
					  sizeof(comp_eq_fir_info)));
}

DECLARE_MODULE(sys_comp_eq_fir_init);
