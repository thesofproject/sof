// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/kpb.h>
#include <sof/common.h>
#include <sof/compiler_attributes.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <rtos/wait.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#if CONFIG_IPC_MAJOR_4
#include <ipc4/detect_test.h>
#include <ipc4/notification.h>
#endif
#include <kernel/abi.h>
#include <sof/samples/audio/detect_test.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sof/samples/audio/kwd_nn_detect_test.h>
#if CONFIG_AMS
#include <sof/lib/ams.h>
#include <sof/lib/ams_msg.h>
#include <ipc4/ams_helpers.h>
#else
#include <sof/lib/notifier.h>
#endif

#define ACTIVATION_DEFAULT_SHIFT 3
#define ACTIVATION_DEFAULT_COEF 0.05

#define ACTIVATION_DEFAULT_THRESHOLD_S16 \
	Q_CONVERT_FLOAT(ACTIVATION_DEFAULT_COEF, 15) /* Q1.15 */
#define ACTIVATION_DEFAULT_THRESHOLD_S24 \
		Q_CONVERT_FLOAT(ACTIVATION_DEFAULT_COEF, 23) /* Q1.23 */
#define ACTIVATION_DEFAULT_THRESHOLD_S32 \
		Q_CONVERT_FLOAT(ACTIVATION_DEFAULT_COEF, 31) /* Q1.31 */

#define INITIAL_MODEL_DATA_SIZE 64

/* default number of samples before detection is activated  */
#define KEYPHRASE_DEFAULT_PREAMBLE_LENGTH 0

#define KWD_NN_BUFF_ALIGN	64

static const struct comp_driver comp_keyword;

LOG_MODULE_REGISTER(kd_test, CONFIG_SOF_LOG_LEVEL);

/* eba8d51f-7827-47b5-82ee-de6e7743af67 */
DECLARE_SOF_RT_UUID("kd-test", keyword_uuid, 0xeba8d51f, 0x7827, 0x47b5,
		 0x82, 0xee, 0xde, 0x6e, 0x77, 0x43, 0xaf, 0x67);

DECLARE_TR_CTX(keyword_tr, SOF_UUID(keyword_uuid), LOG_LEVEL_INFO);

struct comp_data {
#ifdef CONFIG_IPC_MAJOR_4
	struct ipc4_base_module_cfg base_cfg;
#endif
	struct sof_detect_test_config config;
	struct comp_data_blob_handler *model_handler;
	void *data_blob;
	size_t data_blob_size;
	uint32_t data_blob_crc;

	int32_t activation;
	uint32_t detected;
	uint32_t detect_preamble; /**< current keyphrase preamble length */
	uint32_t keyphrase_samples; /**< keyphrase length in samples */
	uint32_t drain_req; /** defines draining size in bytes. */
	uint16_t sample_valid_bytes;
	struct kpb_client client_data;

#if CONFIG_KWD_NN_SAMPLE_KEYPHRASE
	int16_t *input;
	size_t input_size;
#endif

	struct ipc_msg *msg;	/**< host notification */

	void (*detect_func)(struct comp_dev *dev,
			    const struct audio_stream *source, uint32_t frames);
	struct sof_ipc_comp_event event;

#if CONFIG_AMS
	uint32_t kpd_uuid_id;
#else
	struct kpb_event_data event_data;
#endif /* CONFIG_AMS */
};

static inline bool detector_is_sample_width_supported(enum sof_ipc_frame sf)
{
	bool ret;

	switch (sf) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		/* FALLTHRU */
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		/* FALLTHRU */
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		/* FALLTHRU */
#endif /* CONFIG_FORMAT_S32LE */
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

static void notify_host(const struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "notify_host()");

#if CONFIG_IPC_MAJOR_4
	ipc_msg_send(cd->msg, NULL, true);
#else
	ipc_msg_send(cd->msg, &cd->event, true);
#endif /* CONFIG_IPC_MAJOR_4 */
}

#if CONFIG_AMS

/* Key-phrase detected message*/
static const ams_uuid_t ams_kpd_msg_uuid = AMS_KPD_MSG_UUID;

static int ams_notify_kpb(const struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct ams_message_payload ams_payload;

	cd->client_data.r_ptr = NULL;
	cd->client_data.sink = NULL;
	cd->client_data.id = 0; /**< TODO: acquire proper id from kpb */
	/* time in milliseconds */
	cd->client_data.drain_req = (cd->drain_req != 0) ?
				     cd->drain_req :
				     cd->config.drain_req;

	ams_helper_prepare_payload(dev, &ams_payload, cd->kpd_uuid_id,
				   (uint8_t *)&cd->client_data,
				   sizeof(struct kpb_client));

	return ams_send(&ams_payload);
}
#else
static void notify_kpb(const struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "notify_kpb(), preamble: %u", cd->detect_preamble);

	cd->client_data.r_ptr = NULL;
	cd->client_data.sink = NULL;
	cd->client_data.id = 0; /**< TODO: acquire proper id from kpb */
	/* time in milliseconds */
	cd->client_data.drain_req = (cd->drain_req != 0) ?
					 cd->drain_req :
					 cd->config.drain_req;
	cd->event_data.event_id = KPB_EVENT_BEGIN_DRAINING;
	cd->event_data.client_data = &cd->client_data;

	notifier_event(dev, NOTIFIER_ID_KPB_CLIENT_EVT,
		       NOTIFIER_TARGET_CORE_ALL_MASK, &cd->event_data,
		       sizeof(cd->event_data));
}
#endif /* CONFIG_AMS */

void detect_test_notify(const struct comp_dev *dev)
{
	notify_host(dev);
#if CONFIG_AMS
	ams_notify_kpb(dev);
#else
	notify_kpb(dev);
#endif
}

static void default_detect_test(struct comp_dev *dev,
				const struct audio_stream *source,
				uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	void *src;
	int32_t diff;
	uint32_t count = frames; /**< Assuming single channel */
	uint32_t sample;
	uint16_t valid_bits = cd->sample_valid_bytes * 8;
	const int32_t activation_threshold = cd->config.activation_threshold;
	uint32_t cycles_per_frame; /**< Clock cycles required per frame */

	/* synthetic load */
	if (cd->config.load_mips) {
		/* assuming count is a processing frame size in samples */
		cycles_per_frame = (cd->config.load_mips * 1000000 * count)
				   / audio_stream_get_rate(source);
		idelay(cycles_per_frame);
	}

	/* perform detection within current period */
	for (sample = 0; sample < count && !cd->detected; ++sample) {
		src = (valid_bits == 16U) ?
		      audio_stream_read_frag_s16(source, sample) :
		      audio_stream_read_frag_s32(source, sample);
		if (valid_bits > 16U) {
			diff = abs(*(int32_t *)src) - abs(cd->activation);
		} else {
			diff = abs(*(int16_t *)src) -
			       abs((int16_t)cd->activation);
		}

		diff >>= cd->config.activation_shift;
		cd->activation += diff;

		if (cd->detect_preamble >= cd->keyphrase_samples) {
			if (cd->activation >= activation_threshold) {
				/* The algorithm shall use cd->drain_req
				 * to specify its draining size request.
				 * Zero value means default config value
				 * will be used.
				 */
				cd->drain_req = 0;
				detect_test_notify(dev);
				cd->detected = 1;
			}
		} else {
			++cd->detect_preamble;
		}
	}
}

static int test_keyword_get_threshold(struct comp_dev *dev, int sample_width)
{
	switch (sample_width) {
#if CONFIG_FORMAT_S16LE
	case 16:
		return ACTIVATION_DEFAULT_THRESHOLD_S16;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case 24:
		return ACTIVATION_DEFAULT_THRESHOLD_S24;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case 32:
		return ACTIVATION_DEFAULT_THRESHOLD_S32;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(dev, "test_keyword_get_threshold(), unsupported sample width: %d",
			 sample_width);
		return -EINVAL;
	}
}

static int test_keyword_apply_config(struct comp_dev *dev,
				     const struct sof_detect_test_config *cfg)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint16_t sample_width;
	int ret;

	ret = memcpy_s(&cd->config, sizeof(cd->config), cfg,
		       sizeof(struct sof_detect_test_config));
	assert(!ret);

#if CONFIG_IPC_MAJOR_4
	sample_width = cd->base_cfg.audio_fmt.depth;
#else
	sample_width = cd->config.sample_width;
#endif /* CONFIG_IPC_MAJOR_4 */

	if (!cd->config.activation_shift)
		cd->config.activation_shift = ACTIVATION_DEFAULT_SHIFT;

	if (!cd->config.activation_threshold) {
		ret = test_keyword_get_threshold(dev, sample_width);
		if (ret < 0) {
			comp_err(dev, "test_keyword_apply_config(): unsupported sample width %u",
				 sample_width);
			return ret;
		}

		cd->config.activation_threshold = ret;
	}

	return 0;
}

#if CONFIG_IPC_MAJOR_4
#define NOTIFICATION_DEFAULT_WORD_ID 1
#define NOTIFICATION_DEFAULT_SCORE 100

static void test_keyword_set_params(struct comp_dev *dev,
				    struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	enum sof_ipc_frame valid_fmt, frame_fmt;

	comp_info(dev, "test_keyword_set_params()");

	memset(params, 0, sizeof(*params));
	params->channels = cd->base_cfg.audio_fmt.channels_count;
	params->rate = cd->base_cfg.audio_fmt.sampling_frequency;
	params->sample_container_bytes = cd->base_cfg.audio_fmt.depth / 8;
	params->sample_valid_bytes =
		cd->base_cfg.audio_fmt.valid_bit_depth / 8;
	params->buffer_fmt = cd->base_cfg.audio_fmt.interleaving_style;
	params->buffer.size = cd->base_cfg.ibs;

	audio_stream_fmt_conversion(cd->base_cfg.audio_fmt.depth,
				    cd->base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    cd->base_cfg.audio_fmt.s_type);

	params->frame_fmt = valid_fmt;
}

static int test_keyword_set_config(struct comp_dev *dev, const char *data,
				   uint32_t data_size)
{
	const struct sof_detect_test_config *cfg;
	size_t cfg_size;

	/* Copy new config */
	cfg = (const struct sof_detect_test_config *)data;
	cfg_size = data_size;

	comp_info(dev, "test_keyword_set_config(): config size = %u",
		  cfg_size);

	if (cfg_size != sizeof(struct sof_detect_test_config)) {
		comp_err(dev, "test_keyword_set_config(): invalid config size");
		return -EINVAL;
	}

	return test_keyword_apply_config(dev, cfg);
}

static int test_keyword_get_config(struct comp_dev *dev, char *data,
				   uint32_t *data_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	size_t cfg_size;
	int ret;

	comp_info(dev, "test_keyword_get_config()");

	cfg_size = sizeof(struct sof_detect_test_config);

	if (cfg_size > *data_size) {
		comp_err(dev, "test_keyword_get_config(): wrong config size: %d",
			 *data_size);
		return -EINVAL;
	}

	*data_size = cfg_size;

	/* Copy back to user space */
	ret = memcpy_s(data, cfg_size, &cd->config, cfg_size);
	assert(!ret);

	return 0;
}

static int test_keyword_set_large_config(struct comp_dev *dev,
					 uint32_t param_id,
					 bool first_block,
					 bool last_block,
					 uint32_t data_offset,
					 const char *data)
{
	comp_dbg(dev, "test_keyword_set_large_config()");
	struct comp_data *cd = comp_get_drvdata(dev);

	switch (param_id) {
	case IPC4_DETECT_TEST_SET_MODEL_BLOB:
		return ipc4_comp_data_blob_set(cd->model_handler,
					       first_block,
					       last_block,
					       data_offset,
					       data);
	case IPC4_DETECT_TEST_SET_CONFIG:
		return test_keyword_set_config(dev, data, data_offset);
	default:
		return -EINVAL;
	}
}

static int test_keyword_get_large_config(struct comp_dev *dev,
					 uint32_t param_id,
					 bool first_block,
					 bool last_block,
					 uint32_t *data_offset,
					 char *data)
{
	comp_dbg(dev, "test_keyword_get_large_config()");

	switch (param_id) {
	case IPC4_DETECT_TEST_GET_CONFIG:
		return test_keyword_get_config(dev, data, data_offset);
	default:
		return -EINVAL;
	}
}

static int test_keyword_get_attribute(struct comp_dev *dev,
				      uint32_t type,
				      void *value)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		*(struct ipc4_base_module_cfg *)value = cd->base_cfg;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct ipc_msg *ipc4_kd_notification_init(uint32_t word_id,
						 uint32_t score)
{
	struct ipc4_voice_cmd_notification notif;
	struct ipc_msg *msg;

	memset_s(&notif, sizeof(notif), 0, sizeof(notif));

	notif.primary.r.word_id = word_id;
	notif.primary.r.notif_type = SOF_IPC4_NOTIFY_PHRASE_DETECTED;
	notif.primary.r.type = SOF_IPC4_GLB_NOTIFICATION;
	notif.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	notif.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;

	notif.extension.r.sv_score = score;

	msg = ipc_msg_w_ext_init(notif.primary.dat,
				 notif.extension.dat,
				 0);
	if (!msg)
		return NULL;

	return msg;
}

#else /* CONFIG_IPC_MAJOR_4 */
static int test_keyword_set_config(struct comp_dev *dev,
				   struct sof_ipc_ctrl_data *cdata)
{
	struct sof_detect_test_config *cfg;
	size_t bs;

	/* Copy new config, find size from header */
	cfg = (struct sof_detect_test_config *)cdata->data->data;
	bs = cfg->size;

	comp_info(dev, "test_keyword_set_config(), blob size = %u", bs);

	if (bs != sizeof(struct sof_detect_test_config)) {
		comp_err(dev, "test_keyword_set_config(): invalid blob size");
		return -EINVAL;
	}

	return test_keyword_apply_config(dev, cfg);
}

static void test_keyword_set_params(struct comp_dev *dev,
				    struct sof_ipc_stream_params *params)
{}

static int test_keyword_ctrl_set_bin_data(struct comp_dev *dev,
					  struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	if (dev->state != COMP_STATE_READY) {
		/* It is a valid request but currently this is not
		 * supported during playback/capture. The driver will
		 * re-send data in next resume when idle and the new
		 * configuration will be used when playback/capture
		 * starts.
		 */
		comp_err(dev, "keyword_ctrl_set_bin_data(): driver is busy");
		return -EBUSY;
	}

	switch (cdata->data->type) {
	case SOF_DETECT_TEST_CONFIG:
		ret = test_keyword_set_config(dev, cdata);
		break;
	case SOF_DETECT_TEST_MODEL:
		ret = comp_data_blob_set_cmd(cd->model_handler, cdata);
		break;
	default:
		comp_err(dev, "keyword_ctrl_set_bin_data(): unknown binary data type");
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
		comp_err(dev, "test_keyword_cmd_set_data(): invalid version");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_info(dev, "test_keyword_cmd_set_data(), SOF_CTRL_CMD_ENUM");
		break;
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "test_keyword_cmd_set_data(), SOF_CTRL_CMD_BINARY");
		ret = test_keyword_ctrl_set_bin_data(dev, cdata);
		break;
	default:
		comp_err(dev, "test_keyword_cmd_set_data(): invalid cdata->cmd");
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

	comp_info(dev, "test_keyword_get_config()");

	/* Copy back to user space */
	bs = cd->config.size;
	comp_info(dev, "value of block size: %u", bs);

	if (bs == 0 || bs > size)
		return -EINVAL;

	ret = memcpy_s(cdata->data->data, size, &cd->config, bs);
	assert(!ret);

	cdata->data->abi = SOF_ABI_VERSION;
	cdata->data->size = bs;

	return ret;
}

static int test_keyword_ctrl_get_bin_data(struct comp_dev *dev,
					  struct sof_ipc_ctrl_data *cdata,
					  int size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->data->type) {
	case SOF_DETECT_TEST_CONFIG:
		ret = test_keyword_get_config(dev, cdata, size);
		break;
	case SOF_DETECT_TEST_MODEL:
		ret = comp_data_blob_get_cmd(cd->model_handler, cdata, size);
		break;
	default:
		comp_err(dev, "test_keyword_ctrl_get_bin_data(): unknown binary data type");
		break;
	}

	return ret;
}

static int test_keyword_ctrl_get_data(struct comp_dev *dev,
				      struct sof_ipc_ctrl_data *cdata, int size)
{
	int ret = 0;

	comp_info(dev, "test_keyword_ctrl_get_data() size: %d", size);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		ret = test_keyword_ctrl_get_bin_data(dev, cdata, size);
		break;
	default:
		comp_err(dev, "test_keyword_ctrl_get_data(): invalid cdata->cmd");
		return -EINVAL;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int test_keyword_cmd(struct comp_dev *dev, int cmd, void *data,
			    int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);

	comp_info(dev, "test_keyword_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		return test_keyword_ctrl_set_data(dev, cdata);
	case COMP_CMD_GET_DATA:
		return test_keyword_ctrl_get_data(dev, cdata, max_data_size);
	default:
		return -EINVAL;
	}
}
#endif /* CONFIG_IPC_MAJOR_4 */

static struct comp_dev *test_keyword_new(const struct comp_driver *drv,
					 const struct comp_ipc_config *config,
					 const void *spec)
{
	struct comp_dev *dev = NULL;
#if CONFIG_IPC_MAJOR_4
	const struct ipc4_base_module_cfg *base_cfg = spec;
#else
	const struct ipc_config_process *ipc_keyword = spec;
	const struct sof_detect_test_config *cfg;
	size_t bs;
#endif /* CONFIG_IPC_MAJOR_4 */
	struct comp_data *cd = NULL;
	int ret = 0;

	comp_cl_info(&comp_keyword, "test_keyword_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));

	if (!cd)
		goto fail;

#if CONFIG_KWD_NN_SAMPLE_KEYPHRASE
	cd->detect_func = kwd_nn_detect_test;
#else
	/* using default processing function */
	cd->detect_func = default_detect_test;
#endif

	comp_set_drvdata(dev, cd);

	/* component model data handler */
	cd->model_handler = comp_data_blob_handler_new(dev);

#if CONFIG_IPC_MAJOR_4
	/* For IPC4 we only receive the base_cfg, make a copy of it */
	memcpy_s(&cd->base_cfg, sizeof(cd->base_cfg), base_cfg, sizeof(*base_cfg));
#else
	cfg = (const struct sof_detect_test_config *)ipc_keyword->data;
	bs = ipc_keyword->size;

	if (bs > 0) {
		if (bs < sizeof(struct sof_detect_test_config)) {
			comp_err(dev, "test_keyword_new(): invalid data size");
			goto cd_fail;
		}

		if (test_keyword_apply_config(dev, cfg)) {
			comp_err(dev, "test_keyword_new(): failed to apply config");
			goto cd_fail;
		}
	}
#endif /* CONFIG_IPC_MAJOR_4 */

	ret = comp_init_data_blob(cd->model_handler, INITIAL_MODEL_DATA_SIZE,
				  NULL);
	if (ret < 0) {
		comp_err(dev, "test_keyword_new(): model data initial failed");
		goto cd_fail;
	}

	/* build component event */
	ipc_build_comp_event(&cd->event, dev->ipc_config.type, dev->ipc_config.id);
	cd->event.event_type = SOF_CTRL_EVENT_KD;
	cd->event.num_elems = 0;

#if CONFIG_IPC_MAJOR_4
	cd->msg = ipc4_kd_notification_init(NOTIFICATION_DEFAULT_WORD_ID,
					    NOTIFICATION_DEFAULT_SCORE);
#else
	cd->msg = ipc_msg_init(cd->event.rhdr.hdr.cmd, sizeof(cd->event));
#endif /* CONFIG_IPC_MAJOR_4 */

	if (!cd->msg) {
		comp_err(dev, "test_keyword_new(): ipc notification init failed");
		goto cd_fail;
	}

#if CONFIG_KWD_NN_SAMPLE_KEYPHRASE
	/* global buffer to accumulate data for processing */
	cd->input = rballoc_align(0, SOF_MEM_CAPS_RAM,
				  sizeof(int16_t) * KWD_NN_IN_BUFF_SIZE, 64);
	if (!cd->input) {
		comp_err(dev, "test_keyword_new(): input alloc failed");
		goto cd_fail;
	}
	bzero(cd->input, sizeof(int16_t) * KWD_NN_IN_BUFF_SIZE);
	cd->input_size = 0;
#endif

	dev->direction = SOF_IPC_STREAM_CAPTURE;
	dev->direction_set = true;
	dev->state = COMP_STATE_READY;
	return dev;

cd_fail:
	comp_data_blob_handler_free(cd->model_handler);
	rfree(cd);
fail:
	rfree(dev);
	return NULL;
}

static void test_keyword_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "test_keyword_free()");

#if CONFIG_AMS
	int ret;

	/* Unregister KD as AMS producer */
	ret = ams_helper_unregister_producer(dev, cd->kpd_uuid_id);
	if (ret)
		comp_err(dev, "test_keyword_free(): unregister ams error %d", ret);
#endif

	ipc_msg_free(cd->msg);
	comp_data_blob_handler_free(cd->model_handler);
	rfree(cd);
	rfree(dev);
}

static int test_keyword_verify_params(struct comp_dev *dev,
				      struct sof_ipc_stream_params *params)
{
	int ret;

	comp_dbg(dev, "test_keyword_verify_params()");

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "test_keyword_verify_params(): verification failed!");
		return ret;
	}

	return 0;
}

/* set component audio stream parameters */
static int test_keyword_params(struct comp_dev *dev,
			       struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;
	unsigned int channels, rate;
	enum sof_ipc_frame frame_fmt;
	int err;

	test_keyword_set_params(dev, params);

	err = test_keyword_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "test_keyword_params(): pcm params verification failed.");
		return err;
	}

	cd->sample_valid_bytes = params->sample_valid_bytes;

	/* keyword components will only ever have 1 source */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	channels = audio_stream_get_channels(&sourceb->stream);
	frame_fmt = audio_stream_get_frm_fmt(&sourceb->stream);
	rate = audio_stream_get_rate(&sourceb->stream);

	if (channels != 1) {
		comp_err(dev, "test_keyword_params(): only single-channel supported");
		return -EINVAL;
	}

	if (!detector_is_sample_width_supported(frame_fmt)) {
		comp_err(dev, "test_keyword_params(): only 16-bit format supported");
		return -EINVAL;
	}

	/* calculate the length of the preamble */
	if (cd->config.preamble_time) {
		cd->keyphrase_samples = cd->config.preamble_time *
					(rate / 1000);
	} else {
		cd->keyphrase_samples = KEYPHRASE_DEFAULT_PREAMBLE_LENGTH;
	}

	err = test_keyword_get_threshold(dev, params->sample_valid_bytes * 8);
	if (err < 0) {
		comp_err(dev, "test_keyword_params(): unsupported sample width %u",
			 params->sample_valid_bytes * 8);
		return err;
	}

#if CONFIG_AMS
	cd->kpd_uuid_id = AMS_INVALID_MSG_TYPE;
#endif /* CONFIG_AMS */

	cd->config.activation_threshold = err;

	return 0;
}

static int test_keyword_trigger(struct comp_dev *dev, int cmd)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "test_keyword_trigger()");

	ret = comp_set_state(dev, cmd);
	if (ret)
		return ret;

	if (cmd == COMP_TRIGGER_START ||
	    cmd == COMP_TRIGGER_RELEASE) {
		cd->detect_preamble = 0;
		cd->detected = 0;
		cd->activation = 0;
	}

	return 0;
}

/*  process stream data from source buffer */
static int test_keyword_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	uint32_t frames;

	comp_dbg(dev, "test_keyword_copy()");

	/* keyword components will only ever have 1 source */
	source = list_first_item(&dev->bsource_list,
				 struct comp_buffer, sink_list);

	if (!audio_stream_get_avail(&source->stream))
		return PPL_STATUS_PATH_STOP;

	frames = audio_stream_get_avail_frames(&source->stream);

	/* copy and perform detection */
	buffer_stream_invalidate(source, audio_stream_get_avail_bytes(&source->stream));
	cd->detect_func(dev, &source->stream, frames);

	/* calc new available */
	comp_update_buffer_consume(source, audio_stream_get_avail_bytes(&source->stream));

	return 0;
}

static int test_keyword_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "test_keyword_reset()");

	cd->activation = 0;
	cd->detect_preamble = 0;
	cd->detected = 0;

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int test_keyword_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint16_t valid_bits = cd->sample_valid_bytes * 8;
	uint16_t sample_width;
	int ret;

#if CONFIG_IPC_MAJOR_4
	sample_width = cd->base_cfg.audio_fmt.depth;
#else
	sample_width = cd->config.sample_width;
#endif /* CONFIG_IPC_MAJOR_4 */

	comp_info(dev, "test_keyword_prepare()");

	if (valid_bits != sample_width) {
		/* Default threshold value has to be changed
		 * according to host new format.
		 */
		ret = test_keyword_get_threshold(dev, valid_bits);

		if (ret < 0) {
			comp_err(dev, "test_keyword_prepare(): unsupported sample width %u",
				 valid_bits);
			return ret;
		}

		cd->config.activation_threshold = ret;
	}

	cd->data_blob = comp_get_data_blob(cd->model_handler,
					   &cd->data_blob_size,
					   &cd->data_blob_crc);

#if CONFIG_AMS
	/* Register KD as AMS producer */
	ret = ams_helper_register_producer(dev, &cd->kpd_uuid_id,
					   ams_kpd_msg_uuid);
	if (ret)
		return ret;
#endif

	return comp_set_state(dev, COMP_TRIGGER_PREPARE);
}

uint16_t test_keyword_get_sample_valid_bytes(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	return cd->sample_valid_bytes;
}

uint32_t test_keyword_get_detected(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	return cd->detected;
}

void test_keyword_set_detected(struct comp_dev *dev, uint32_t detected)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	cd->detected = detected;
}

#if CONFIG_KWD_NN_SAMPLE_KEYPHRASE
const int16_t *test_keyword_get_input(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	return cd->input;
}

int16_t test_keyword_get_input_byte(struct comp_dev *dev, uint32_t index)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	if (index >= KWD_NN_IN_BUFF_SIZE * sizeof(int16_t))
		return -EINVAL;

	return *((unsigned char *)cd->input + index);
}

int16_t test_keyword_get_input_elem(struct comp_dev *dev, uint32_t index)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	if (index >= KWD_NN_IN_BUFF_SIZE)
		return -EINVAL;
	return cd->input[index];
}

int test_keyword_set_input_elem(struct comp_dev *dev, uint32_t index, int16_t val)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	if (index >= KWD_NN_IN_BUFF_SIZE)
		return -EINVAL;

	cd->input[index] = val;

	return 0;
}

size_t test_keyword_get_input_size(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	return cd->input_size;
}

void test_keyword_set_input_size(struct comp_dev *dev, size_t input_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	cd->input_size = input_size;
}
#endif

uint32_t test_keyword_get_drain_req(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	return cd->drain_req;
}

void test_keyword_set_drain_req(struct comp_dev *dev, uint32_t drain_req)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	cd->drain_req = drain_req;
}

static const struct comp_driver comp_keyword = {
	.type	= SOF_COMP_KEYWORD_DETECT,
	.uid	= SOF_RT_UUID(keyword_uuid),
	.tctx	= &keyword_tr,
	.ops	= {
		.create			= test_keyword_new,
		.free			= test_keyword_free,
		.params			= test_keyword_params,
#if CONFIG_IPC_MAJOR_4
		.set_large_config	= test_keyword_set_large_config,
		.get_large_config	= test_keyword_get_large_config,
		.get_attribute		= test_keyword_get_attribute,
#else
		.cmd			= test_keyword_cmd,
#endif /* CONFIG_IPC_MAJOR_4 */
		.trigger		= test_keyword_trigger,
		.copy			= test_keyword_copy,
		.prepare		= test_keyword_prepare,
		.reset			= test_keyword_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_keyword_info = {
	.drv = &comp_keyword,
};

UT_STATIC void sys_comp_keyword_init(void)
{
	comp_register(platform_shared_get(&comp_keyword_info,
					  sizeof(comp_keyword_info)));
}

DECLARE_MODULE(sys_comp_keyword_init);
SOF_MODULE_INIT(keyword, sys_comp_keyword_init);
