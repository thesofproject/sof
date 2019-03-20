/*
 * Copyright (c) 2019, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/ipc.h>
#include <sof/notifier.h>
#include <sof/audio/component.h>
#include <sof/audio/kpb.h>
#include <uapi/user/detect_test.h>

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

/* number of samples to be treated as a full keyphrase */
#define KEYPHRASE_DEFAULT_PREAMBLE_LENGTH (30 * 1024)

struct comp_data {
	struct sof_detect_test_config config;
	void *load_memory;	/**< synthetic memory load */
	int16_t activation;
	uint32_t detected;
	uint32_t detect_preamble; /**< current keyphrase preamble length */
	uint32_t keyphrase_samples; /**< keyphrase length in samples */
	uint32_t buf_copy_pos; /**< current copy position for incoming data */

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
	cd->client_data.history_end = 0; /**< keyphrase end, 0 is now */
	cd->client_data.history_begin = cd->detect_preamble;
	/* time in milliseconds */
	cd->client_data.history_depth = cd->detect_preamble /
					(dev->params.rate / 1000);

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

	/* synthetic load */
	if (cd->config.load_mips)
		idelay(cd->config.load_mips * 1000000);

	/* perform detection within current period */
	for (sample = 0; sample < count && !cd->detected; ++sample) {
		src = buffer_read_frag_s16(source, sample);
		diff = abs(*src) - cd->activation;
		step = diff >> cd->config.activation_shift;

		/* prevent taking 0 steps when the diff is too low */
		cd->activation += !step ? diff : step;

		if (cd->detect_preamble >= cd->keyphrase_samples) {
			if (cd->activation >= cd->config.activation_threshold) {
				detect_test_notify(dev);
				cd->detected = 1;
			}
		} else {
			++cd->detect_preamble;
		}
	}
}

static void free_mem_load(struct comp_data *cd)
{
	if (!cd) {
		trace_keyword_error("free_mem_load() error: invalid cd");
		return;
	}

	if (cd->load_memory) {
		rfree(cd->load_memory);
		cd->load_memory = NULL;
	}
}

static int alloc_mem_load(struct comp_data *cd, uint32_t size)
{
	if (!cd) {
		trace_keyword_error("alloc_mem_load() error: invalid cd");
		return -EINVAL;
	}

	free_mem_load(cd);

	if (!size)
		return 0;

	cd->load_memory = rmalloc(RZONE_BUFFER, SOF_MEM_CAPS_RAM, size);

	if (!cd->load_memory) {
		trace_keyword_error("alloc_mem_load() alloc failed");
		return -ENOMEM;
	}

	bzero(cd->load_memory, size);
	cd->buf_copy_pos = 0;

	return 0;
}

static void test_keyword_set_default_config(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	memset(&cd->config, 0, sizeof(cd->config));

	cd->keyphrase_samples = KEYPHRASE_DEFAULT_PREAMBLE_LENGTH;
	cd->config.activation_shift = ACTIVATION_DEFAULT_SHIFT;
	cd->config.activation_threshold = ACTIVATION_DEFAULT_THRESHOLD_S16;
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
	memcpy_s(keyword, sizeof(*keyword), ipc_keyword,
		 sizeof(struct sof_ipc_comp_process));

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));

	if (!cd) {
		rfree(dev);
		return NULL;
	}

	/* using default processing function */
	cd->detect_func = default_detect_test;

	test_keyword_set_default_config(dev);

	comp_set_drvdata(dev, cd);
	dev->state = COMP_STATE_READY;
	return dev;
}

static void test_keyword_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_keyword("test_keyword_free()");

	free_mem_load(cd);
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

	dev->frame_bytes = comp_frame_bytes(dev);

	/* calculate the length of the preamble */
	if (cd->config.keyphrase_length) {
		if (cd->config.keyphrase_length > KPB_MAX_BUFF_TIME) {
			trace_keyword_error("test_keyword_params() "
					    "error: kp length too long");
			return -EINVAL;
		}

		cd->keyphrase_samples = cd->config.keyphrase_length *
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
	struct sof_detect_test_config *cfg;
	size_t bs;

	/* Copy new config, find size from header */
	cfg = (struct sof_detect_test_config *)cdata->data->data;
	bs = cfg->size;

	trace_keyword("test_keyword_set_config(), blob size = %u",
		      bs);

	if (bs > SOF_DETECT_TEST_MAX_CFG_SIZE || bs == 0) {
		trace_keyword_error("test_keyword_set_config() "
				    "error: invalid blob size");
		return -EINVAL;
	}

	memcpy_s(&cd->config, sizeof(cd->config), cdata->data->data, bs);

	if (!cd->config.activation_shift)
		cd->config.activation_shift = ACTIVATION_DEFAULT_SHIFT;

	if (!cd->config.activation_threshold)
		cd->config.activation_threshold =
			ACTIVATION_DEFAULT_THRESHOLD_S16;

	return alloc_mem_load(cd, cd->config.load_memory_size);
}

static int test_keyword_set_model(struct comp_dev *dev,
				  struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	if (!cd->load_memory) {
		trace_keyword_error("keyword_ctrl_set_model() error: "
				   "buffer not allocated");
		return -EINVAL;
	}

	if (!cdata->elems_remaining) {
		if (cdata->data->size + cd->buf_copy_pos <
		    cd->config.load_memory_size) {
			trace_keyword_error("keyword_ctrl_set_model() error: "
					   "not enough data to fill the buffer");

			/* TODO: anything to do in such a situation? */

			return -EINVAL;
		}

		trace_keyword("test_keyword_set_model() "
			      "final packet received");
	}

	if (cdata->data->size >
	    cd->config.load_memory_size - cd->buf_copy_pos) {
		trace_keyword_error("keyword_ctrl_set_model() error: "
				   "too much data");
		return -EINVAL;
	}

	memcpy_s(cd->load_memory + cd->buf_copy_pos,
		 cd->config.load_memory_size - cd->buf_copy_pos,
		 cdata->data->data, cdata->data->size);

	cd->buf_copy_pos += cdata->data->size;

	return 0;
}

static int test_keyword_ctrl_set_bin_data(struct comp_dev *dev,
					  struct sof_ipc_ctrl_data *cdata)
{
	int ret = 0;

	if (dev->state != COMP_STATE_READY) {
		/* It is a valid request but currently this is not
		 * supported during playback/capture. The driver will
		 * re-send data in next resume when idle and the new
		 * configuration will be used when playback/capture
		 * starts.
		 */
		trace_keyword_error("keyword_ctrl_set_bin_data() error: "
				   "driver is busy");
		return -EBUSY;
	}

	switch (cdata->data->type) {
	case SOF_DETECT_TEST_CONFIG:
		ret = test_keyword_set_config(dev, cdata);
		break;
	case SOF_DETECT_TEST_MODEL:
		ret = test_keyword_set_model(dev, cdata);
		break;
	default:
		trace_keyword_error("keyword_ctrl_set_bin_data() error: "
				   "unknown binary data type");
		break;
	}

	return ret;
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
		ret = test_keyword_ctrl_set_bin_data(dev, cdata);
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

	/* Copy back to user space */
	bs = cd->config.size;
	trace_value(bs);

	if (bs == 0 || bs > size)
		return -EINVAL;

	memcpy_s(cdata->data->data, size, &cd->config, bs);
	cdata->data->abi = SOF_ABI_VERSION;
	cdata->data->size = bs;

	return ret;
}

static int test_keyword_get_model(struct comp_dev *dev,
				  struct sof_ipc_ctrl_data *cdata, int size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	size_t bs;
	uint32_t crc;
	int ret = 0;

	trace_keyword("test_keyword_get_model()");

	/* Copy back to user space */
	if (cd->load_memory) {
		bs = sizeof(uint32_t);

		if (bs > size) {
			trace_keyword_error("test_keyword_get_model() error: "
					    "invalid size %d", bs);
			return -EINVAL;
		}

		crc = crc32(cd->load_memory, cd->config.load_memory_size);

		trace_keyword("test_keyword_get_model() crc: 0x%X", crc);

		memcpy_s(cdata->data->data, size, &crc, bs);
		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = bs;
	} else {
		trace_keyword_error("test_keyword_get_model() "
				    "error: invalid cd->config");
		ret = -EINVAL;
	}

	return ret;
}

static int test_keyword_ctrl_get_bin_data(struct comp_dev *dev,
					  struct sof_ipc_ctrl_data *cdata,
					  int size)
{
	int ret = 0;

	switch (cdata->data->type) {
	case SOF_DETECT_TEST_CONFIG:
		ret = test_keyword_get_config(dev, cdata, size);
		break;
	case SOF_DETECT_TEST_MODEL:
		ret = test_keyword_get_model(dev, cdata, size);
		break;
	default:
		trace_keyword_error("test_keyword_ctrl_get_bin_data() error: "
				   "unknown binary data type");
		break;
	}

	return ret;
}

static int test_keyword_ctrl_get_data(struct comp_dev *dev,
				      struct sof_ipc_ctrl_data *cdata, int size)
{
	int ret = 0;

	trace_keyword("test_keyword_ctrl_get_data() size: %d", size);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		ret = test_keyword_ctrl_get_bin_data(dev, cdata, size);
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

	/* make sure source component buffer has enough data available for copy
	 * Also check for XRUNs
	 */
	if (!source->avail) {
		trace_keyword_error("test_keyword_copy() error: "
				   "source component buffer"
				   " has not enough data available");
		comp_underrun(dev, source, 1, 0);
		return -EIO;	/* xrun */
	}

	/* copy and perform detection */
	cd->detect_func(dev, source,
			source->avail / comp_frame_bytes(source->source));

	/* calc new available */
	comp_update_buffer_consume(source, source->avail);

	return 0;
}

static int test_keyword_reset(struct comp_dev *dev)
{
	trace_keyword("test_keyword_reset()");

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

static void sys_comp_keyword_init(void)
{
	comp_register(&comp_keyword);
}

DECLARE_MODULE(sys_comp_keyword_init);
