// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <stdint.h>
#include <stddef.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/ipc.h>
#include <sof/notifier.h>
#include <sof/ut.h>
#include <sof/audio/component.h>
#include <sof/audio/kpb.h>
#include <user/detect_test.h>

/* tracing */
#define trace_keyword(__e, ...) \
	trace_event(TRACE_CLASS_KEYWORD, __e, ##__VA_ARGS__)
#define trace_keyword_error(__e, ...) \
	trace_error(TRACE_CLASS_KEYWORD, __e, ##__VA_ARGS__)
#define tracev_keyword(__e, ...) \
	tracev_event(TRACE_CLASS_KEYWORD, __e, ##__VA_ARGS__)

#define ACTIVATION_DEFAULT_SHIFT 3
#define ACTIVATION_DEFAULT_THRESHOLD 0.5

#define ACTIVATION_DEFAULT_THRESHOLD_S16 \
	((int16_t)((INT16_MAX) * (ACTIVATION_DEFAULT_THRESHOLD)))

/* default number of samples before detection is activated  */
#define KEYPHRASE_DEFAULT_PREAMBLE_LENGTH 0

struct comp_data {
	struct sof_detect_test_config *config;
	int16_t activation;
	uint32_t detected;
	uint32_t detect_preamble; /**< current keyphrase preamble length */
	uint32_t keyphrase_samples; /**< keyphrase length in samples */
	uint32_t buf_copy_pos; /**< current copy position for incoming data */
	uint32_t history_depth; /** defines draining size in bytes. */

	struct notify_data event;
	struct kpb_event_data event_data;
	struct kpb_client client_data;

	void (*detect_func)(struct comp_dev *dev,
			    struct comp_buffer *source, uint32_t frames);
};

static void notify_host(struct comp_dev *dev)
{
	struct sof_ipc_comp_event event;

	trace_keyword("notify_host()");

	event.event_type = SOF_CTRL_EVENT_KD;
	event.num_elems = 0;

	ipc_send_comp_notification(dev, &event);
}

static void notify_kpb(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_keyword("notify_kpb(), preamble: %u", cd->detect_preamble);

	cd->client_data.r_ptr = NULL;
	cd->client_data.sink = NULL;
	cd->client_data.id = 0; /**< TODO: acquire proper id from kpb */
	/* time in milliseconds */
	cd->client_data.history_depth = (cd->history_depth != 0) ?
					 cd->history_depth :
					 cd->config->history_depth;
	cd->event_data.event_id = KPB_EVENT_BEGIN_DRAINING;
	cd->event_data.client_data = &cd->client_data;

	cd->event.id = NOTIFIER_ID_KPB_CLIENT_EVT;
	cd->event.target_core_mask = NOTIFIER_TARGET_CORE_ALL_MASK;
	cd->event.data = &cd->event_data;

	notifier_event(&cd->event);
}

static void detect_test_notify(struct comp_dev *dev)
{
	notify_host(dev);
	notify_kpb(dev);
}

static void default_detect_test(struct comp_dev *dev,
				struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	int16_t *src;
	int16_t diff;
	int16_t step;
	uint32_t count = frames; /**< Assuming single channel */
	uint32_t sample;
	uint32_t sample_width = dev->params.sample_container_bytes;

	/* TODO: synthetic load */

	/* perform detection within current period */
	for (sample = 0; sample < count && !cd->detected; ++sample) {
		src = (sample_width == 16) ?
		       buffer_read_frag_s16(source, sample) :
		       buffer_read_frag_s32(source, sample);
		diff = abs(*src) - cd->activation;
		step = diff >> cd->config->activation_shift;

		/* prevent taking 0 steps when the diff is too low */
		cd->activation += !step ? diff : step;

		if (cd->detect_preamble >= cd->keyphrase_samples) {
			if (cd->activation >=
			    cd->config->activation_threshold) {
				/* The algorithm shall use cd->history_depth
				 * to specify its draining size request.
				 * Zero value means default config value
				 * will be used.
				 */
				cd->history_depth = 0;
				detect_test_notify(dev);
				cd->detected = 1;
			}
		} else {
			++cd->detect_preamble;
		}
	}
}

static void free_config(struct comp_data *cd)
{
	if (!cd) {
		trace_keyword_error("free_config() error: invalid cd");
		return;
	}

	if (cd->config) {
		rfree(cd->config);
		cd->config = NULL;
	}
}

static int alloc_config(struct comp_data *cd, uint32_t size)
{
	if (!cd) {
		trace_keyword_error("alloc_config() error: invalid cd");
		return -EINVAL;
	}

	free_config(cd);

	if (!size)
		return 0;

	cd->config = rballoc(RZONE_BUFFER, SOF_MEM_CAPS_RAM, size);

	if (!cd->config) {
		trace_keyword_error("alloc_config() alloc failed");
		return -ENOMEM;
	}

	bzero(cd->config, size);
	cd->buf_copy_pos = 0;

	return 0;
}

static void test_keyword_check_defconfig(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	if (!cd->config->activation_shift)
		cd->config->activation_shift = ACTIVATION_DEFAULT_SHIFT;

	if (!cd->config->activation_threshold)
		cd->config->activation_threshold =
			ACTIVATION_DEFAULT_THRESHOLD_S16;
}

static struct comp_dev *test_keyword_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_process *keyword;
	struct sof_ipc_comp_process *ipc_keyword =
		(struct sof_ipc_comp_process *)comp;
	struct comp_data *cd;

	trace_keyword("test_keyword_new()");

	if (IPC_IS_SIZE_INVALID(ipc_keyword->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_KEYWORD, ipc_keyword->config);
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	keyword = (struct sof_ipc_comp_process *)&dev->comp;
	assert(!memcpy_s(keyword, sizeof(*keyword), ipc_keyword,
		 sizeof(struct sof_ipc_comp_process)));

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));

	if (!cd)
		goto fail;

	/* using default processing function */
	cd->detect_func = default_detect_test;

	comp_set_drvdata(dev, cd);

	dev->state = COMP_STATE_READY;
	return dev;

fail:
	if (cd)
		rfree(cd);
	rfree(dev);
	return NULL;
}

static void test_keyword_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_keyword("test_keyword_free()");

	free_config(cd);
	rfree(cd);
	rfree(dev);
}

/* set component audio stream parameters */
static int test_keyword_params(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	/* TODO: remove in the future */
	dev->params.channels = 1;

	if (dev->params.channels != 1) {
		trace_keyword_error("test_keyword_params() "
				    "error: only single-channel supported");
		return -EINVAL;
	}

	if (dev->params.frame_fmt != SOF_IPC_FRAME_S16_LE) {
		trace_keyword_error("test_keyword_params() "
				    "error: only 16-bit format supported");
		return -EINVAL;
	}

	/* calculate the length of the preamble */
	if (cd->config->preamble_time) {
		cd->keyphrase_samples = cd->config->preamble_time *
					(dev->params.rate / 1000);
	} else {
		cd->keyphrase_samples = KEYPHRASE_DEFAULT_PREAMBLE_LENGTH;
	}

	return 0;
}

static int test_keyword_set_config(struct comp_dev *dev,
				   struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t dst_size = 0;

	if (dev->state != COMP_STATE_READY) {
		/* It is a valid request but currently this is not
		 * supported during playback/capture. The driver will
		 * re-send data in next resume when idle and the new
		 * configuration will be used when playback/capture
		 * starts.
		 */
		trace_keyword_error("test_keyword_set_config() error: "
				   "driver is busy");
		return -EBUSY;
	}

	if (!cdata->msg_index) {
		dst_size = cdata->num_elems + cdata->elems_remaining;
		alloc_config(cd, cdata->num_elems + cdata->elems_remaining);

		if (!cd->config) {
			trace_keyword_error("test_keyword_set_config() error: "
					    "buffer allocation failed");

			return -EINVAL;
		}

		/* fallback to defaults if we have unset values */
		test_keyword_check_defconfig(dev);
	} else {
		dst_size = cd->config->size;
		if (!cdata->elems_remaining) {
			if (cdata->num_elems + cd->buf_copy_pos <
			    cd->config->size) {
				trace_keyword_error("test_keyword_set_config() "
						    "error: "
						    "not enough data to fill "
						    "the buffer");
				/* TODO: anything to do in such a situation? */
				return -EINVAL;
			}

			if (cdata->num_elems + cd->buf_copy_pos >
			    cd->config->size) {
				trace_keyword_error("test_keyword_set_config() "
						    "warning: too much data");
			}

			trace_keyword("test_keyword_set_config() "
				      "final packet received");
		}
	}

	assert(!memcpy_s(((void *)cd->config) + cd->buf_copy_pos,
			 dst_size, cdata->data->data, cdata->num_elems));

	cd->buf_copy_pos += cdata->num_elems;

	return 0;
}

static int test_keyword_ctrl_set_data(struct comp_dev *dev,
				      struct sof_ipc_ctrl_data *cdata)
{
	int ret = 0;

	/* Check version from ABI header */
	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, cdata->data->abi)) {
		trace_keyword_error("test_keyword_cmd_set_data() error: "
				    "invalid version");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		trace_keyword("test_keyword_cmd_set_data(), SOF_CTRL_CMD_ENUM");
		break;
	case SOF_CTRL_CMD_BINARY:
		trace_keyword("test_keyword_cmd_set_data(), "
			      "SOF_CTRL_CMD_BINARY");
		ret = test_keyword_set_config(dev, cdata);
		break;
	default:
		trace_keyword_error("test_keyword_cmd_set_data() "
				    "error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int test_keyword_get_config(struct comp_dev *dev,
				   struct sof_ipc_ctrl_data *cdata, int size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	size_t bs;
	int ret = 0;

	trace_keyword("test_keyword_get_config()");

	if (!cd->config)
		return -EINVAL;

	/* Copy back to user space, without the model */
	bs = sizeof(struct sof_detect_test_config);
	trace_value(bs);

	if (bs == 0 || bs + sizeof(uint32_t) > size)
		return -EINVAL;

	struct sof_detect_test_config *cfg =
		(struct sof_detect_test_config *)cdata->data->data;
	uint32_t crc = crc32(cd->config->model, cd->config->model_size);

	assert(!memcpy_s(cfg, size, &cd->config, bs));
	cfg->model[0] = crc;

	/* bs (config size) + uint32_t for crc */
	cdata->data->size = bs + sizeof(uint32_t);
	cdata->data->abi = SOF_ABI_VERSION;

	return ret;
}

static int test_keyword_ctrl_get_data(struct comp_dev *dev,
				      struct sof_ipc_ctrl_data *cdata, int size)
{
	int ret = 0;

	trace_keyword("test_keyword_ctrl_get_data() size: %d", size);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		ret = test_keyword_get_config(dev, cdata, size);
		break;
	default:
		trace_keyword_error("test_keyword_ctrl_get_data() error: "
				    "invalid cdata->cmd");
		return -EINVAL;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int test_keyword_cmd(struct comp_dev *dev, int cmd, void *data,
			    int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;

	trace_keyword("test_keyword_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		return test_keyword_ctrl_set_data(dev, cdata);
	case COMP_CMD_GET_DATA:
		return test_keyword_ctrl_get_data(dev, cdata, max_data_size);
	default:
		return -EINVAL;
	}
}

static int test_keyword_trigger(struct comp_dev *dev, int cmd)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_keyword("test_keyword_trigger()");

	ret = comp_set_state(dev, cmd);
	if (ret)
		return ret;

	if (cmd == COMP_TRIGGER_START ||
	    cmd == COMP_TRIGGER_RELEASE) {
		cd->detect_preamble = 0;
		cd->detected = 0;
		cd->activation = 0;
	}

	return ret;
}

/*  process stream data from source buffer */
static int test_keyword_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;

	tracev_keyword("test_keyword_copy()");

	/* keyword components will only ever have 1 source */
	source = list_first_item(&dev->bsource_list,
				 struct comp_buffer, sink_list);

	/* copy and perform detection */
	cd->detect_func(dev, source,
			source->avail / comp_frame_bytes(dev));

	/* calc new available */
	comp_update_buffer_consume(source, source->avail);

	return 0;
}

static int test_keyword_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_keyword("test_keyword_reset()");

	cd->activation = 0;
	cd->detect_preamble = 0;
	cd->detected = 0;

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int test_keyword_prepare(struct comp_dev *dev)
{
	trace_keyword("test_keyword_prepare()");

	return comp_set_state(dev, COMP_TRIGGER_PREPARE);
}

static void test_keyword_cache(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd;

	switch (cmd) {
	case CACHE_WRITEBACK_INV:
		trace_keyword("test_keyword_cache(), CACHE_WRITEBACK_INV");

		cd = comp_get_drvdata(dev);

		dcache_writeback_invalidate_region(cd, sizeof(*cd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case CACHE_INVALIDATE:
		trace_keyword("test_keyword_cache(), CACHE_INVALIDATE");

		dcache_invalidate_region(dev, sizeof(*dev));

		cd = comp_get_drvdata(dev);
		dcache_invalidate_region(cd, sizeof(*cd));
		break;
	}
}

struct comp_driver comp_keyword = {
	.type	= SOF_COMP_KEYWORD_DETECT,
	.ops	= {
		.new		= test_keyword_new,
		.free		= test_keyword_free,
		.params		= test_keyword_params,
		.cmd		= test_keyword_cmd,
		.trigger	= test_keyword_trigger,
		.copy		= test_keyword_copy,
		.prepare	= test_keyword_prepare,
		.reset		= test_keyword_reset,
		.cache		= test_keyword_cache,
	},
};

UT_STATIC void sys_comp_keyword_init(void)
{
	comp_register(&comp_keyword);
}

DECLARE_MODULE(sys_comp_keyword_init);
