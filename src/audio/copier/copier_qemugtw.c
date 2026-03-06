// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component_ext.h>
#include <sof/trace/trace.h>
#include <sof/lib/memory.h>
#include <sof/ut.h>
#include <rtos/init.h>
#include <sof/math/trig.h>
#include "copier.h"
#include "qemugtw_copier.h"

LOG_MODULE_REGISTER(qemugtw, CONFIG_SOF_LOG_LEVEL);

static struct list_item qemugtw_list_head = LIST_INIT(qemugtw_list_head);

static inline struct comp_buffer *get_buffer(struct comp_dev *dev, struct qemugtw_data *qemugtw_data)
{
	if (qemugtw_data->node_id.f.dma_type == ipc4_qemu_input_class) {
		if (list_is_empty(&dev->bsink_list))
			return NULL;
		return comp_dev_get_first_data_consumer(dev);
	}

	assert(qemugtw_data->node_id.f.dma_type == ipc4_qemu_output_class);

	if (list_is_empty(&dev->bsource_list))
		return NULL;
	return comp_dev_get_first_data_producer(dev);
}

static int qemugtw_generate_signal(struct qemugtw_data *qemugtw_data, struct comp_buffer *buf, uint32_t size)
{
	int32_t *snk;
	uint32_t samples_to_gen = size / sizeof(int32_t);
	uint32_t i;
	int32_t val;
	
	snk = (int32_t *)audio_stream_wrap(&buf->stream, audio_stream_get_wptr(&buf->stream));

	for (i = 0; i < samples_to_gen; i++) {
		switch (qemugtw_data->config.signal_type) {
		case 0: /* Sine */
		{
			/* Simple sine wave approximation using trig.h */
			int32_t th_rad = (int32_t)(((int64_t)qemugtw_data->phase * PI_MUL2_Q4_28) / 360);
			int32_t sin_val = sin_fixed_32b(th_rad);
			val = (int32_t)q_mults_32x32(sin_val, qemugtw_data->config.amplitude, 31);
			break;
		}
		case 1: /* Square */
			val = (qemugtw_data->phase < 180) ? qemugtw_data->config.amplitude : -qemugtw_data->config.amplitude;
			break;
		case 2: /* Saw */
			val = ((int32_t)qemugtw_data->phase * (int32_t)qemugtw_data->config.amplitude) / 360;
			break;
		case 3: /* Linear increment */
			val = qemugtw_data->counter++;
			break;
		default:
			val = 0;
			break;
		}

		/* Update phase */
		qemugtw_data->phase = (qemugtw_data->phase + qemugtw_data->config.frequency) % 360;

		/* Apply to buffer */
		*snk = val;
		snk = (int32_t *)audio_stream_wrap(&buf->stream, (uint8_t *)(snk + 1));
	}
	
	return 0;
}

static int qemugtw_validate_signal(struct qemugtw_data *qemugtw_data, struct comp_buffer *buf, uint32_t size)
{
	int32_t *src;
	uint32_t samples_to_check = size / sizeof(int32_t);
	uint32_t i;
	int32_t expected_val;
	
	src = (int32_t *)audio_stream_wrap(&buf->stream, audio_stream_get_rptr(&buf->stream));

	for (i = 0; i < samples_to_check; i++) {
		switch (qemugtw_data->config.signal_type) {
		case 0: /* Sine */
		{
			int32_t th_rad = (int32_t)(((int64_t)qemugtw_data->phase * PI_MUL2_Q4_28) / 360);
			int32_t sin_val = sin_fixed_32b(th_rad);
			expected_val = (int32_t)q_mults_32x32(sin_val, qemugtw_data->config.amplitude, 31);
			break;
		}
		case 1: /* Square */
			expected_val = (qemugtw_data->phase < 180) ? qemugtw_data->config.amplitude : -qemugtw_data->config.amplitude;
			break;
		case 2: /* Saw */
			expected_val = ((int32_t)qemugtw_data->phase * (int32_t)qemugtw_data->config.amplitude) / 360;
			break;
		case 3: /* Linear increment */
			expected_val = qemugtw_data->counter++;
			break;
		default:
			expected_val = 0;
			break;
		}

		if (*src != expected_val) {
			comp_err(qemugtw_data->dev, "QEMU GTW Validation failed at sample %u. Expected %d, got %d", i, expected_val, *src);
			qemugtw_data->error_count++;
			/* Can add further IPC notification logic here */
		}

		qemugtw_data->validated_bytes += sizeof(int32_t);

		/* Update phase */
		qemugtw_data->phase = (qemugtw_data->phase + qemugtw_data->config.frequency) % 360;
		src = (int32_t *)audio_stream_wrap(&buf->stream, (uint8_t *)(src + 1));
	}
	
	return 0;
}


int copier_qemugtw_process(struct comp_dev *dev)
{
	struct qemugtw_data *qemugtw_data;
	struct comp_buffer *buf;
	uint32_t data_size;

	/* Find gateway mapping */
	qemugtw_data = NULL;
	struct list_item *item;
	list_for_item(item, &qemugtw_list_head) {
		struct qemugtw_data *data = list_item(item, struct qemugtw_data, item);
		if (data->dev == dev) {
			qemugtw_data = data;
			break;
		}
	}

	if (!qemugtw_data)
		return -ENODEV;

	buf = get_buffer(dev, qemugtw_data);
	if (!buf) {
		comp_warn(dev, "no buffer found");
		return 0;
	}

	uint32_t process_bytes;

	if (qemugtw_data->node_id.f.dma_type == ipc4_qemu_input_class) { /* DSP <- GTW, generate */
		process_bytes = audio_stream_period_bytes(&buf->stream, dev->frames);
		data_size = MIN(audio_stream_get_free_bytes(&buf->stream), process_bytes);
		if (data_size > 0) {
			qemugtw_generate_signal(qemugtw_data, buf, data_size);
			buffer_stream_writeback(buf, data_size);
			comp_update_buffer_produce(buf, data_size);
		}
	} else { /* DSP -> GTW, consume and validate */
		process_bytes = audio_stream_period_bytes(&buf->stream, dev->frames);
		data_size = MIN(audio_stream_get_avail_bytes(&buf->stream), process_bytes);
		if (data_size > 0) {
			buffer_stream_invalidate(buf, data_size);
			qemugtw_validate_signal(qemugtw_data, buf, data_size);
			comp_update_buffer_consume(buf, data_size);
		}
	}

	return 0;
}

int copier_qemugtw_params(struct qemugtw_data *qemugtw_data, struct comp_dev *dev,
			 struct sof_ipc_stream_params *params)
{
	struct comp_buffer *buf;
	int err;

	comp_dbg(dev, "qemugtw_params()");

	buf = get_buffer(dev, qemugtw_data);
	if (!buf) {
		comp_err(dev, "no buffer found");
		return -EINVAL;
	}

	err = buffer_set_size(buf, qemugtw_data->buf_size, 0);
	if (err < 0) {
		comp_err(dev, "failed to resize buffer to %u bytes",
			 qemugtw_data->buf_size);
		return err;
	}

	return 0;
}

void copier_qemugtw_reset(struct comp_dev *dev)
{
	struct qemugtw_data *qemugtw_data = NULL;
	struct list_item *item;
	list_for_item(item, &qemugtw_list_head) {
		struct qemugtw_data *data = list_item(item, struct qemugtw_data, item);
		if (data->dev == dev) {
			qemugtw_data = data;
			break;
		}
	}

	if (!qemugtw_data)
		return;

	struct comp_buffer *buf = get_buffer(dev, qemugtw_data);

	if (buf) {
		audio_stream_reset(&buf->stream);
	} else {
		comp_warn(dev, "no buffer found");
	}
	
	/* Reset phases */
	list_for_item(item, &qemugtw_list_head) {
		struct qemugtw_data *data = list_item(item, struct qemugtw_data, item);
		if (data->dev == dev) {
			data->phase = 0;
			data->counter = 0;
			break;
		}
	}
}

__cold int copier_qemugtw_create(struct processing_module *mod,
				const struct ipc4_copier_module_cfg *copier,
				struct pipeline *pipeline)
{
	struct copier_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_ipc_config *config = &dev->ipc_config;
	struct qemugtw_data *qemugtw_data;
	const struct ipc4_copier_gateway_cfg *gtw_cfg;
	const struct ipc4_qemu_gateway_config_blob *blob;
	int ret;

	assert_can_be_cold();

	gtw_cfg = &copier->gtw_cfg;
	if (!gtw_cfg->config_length) {
		comp_err(dev, "empty ipc4_gateway_config_data");
		return -EINVAL;
	}

	cd->qemu_gtw = true;

	/* The QEMU gateway is treated as a host gateway for format conversion */
	config->type = SOF_COMP_HOST;
	cd->gtw_type = ipc4_gtw_qemu;

	qemugtw_data = mod_zalloc(mod, sizeof(*qemugtw_data));
	if (!qemugtw_data)
		return -ENOMEM;

	qemugtw_data->node_id = gtw_cfg->node_id;
	qemugtw_data->dev = dev;

	blob = (const struct ipc4_qemu_gateway_config_blob *)
		((const struct ipc4_gateway_config_data *)gtw_cfg->config_data)->config_blob;

	if (blob) {
		/* Apply blob values */
		qemugtw_data->config = *blob;
	}

	/* Use a static buffer size for testing */
	qemugtw_data->buf_size = 4096;

	cd->converter[IPC4_COPIER_GATEWAY_PIN] =
		get_converter_func(&copier->base.audio_fmt,
				   &copier->out_fmt,
				   ipc4_gtw_qemu, IPC4_DIRECTION(cd->direction), DUMMY_CHMAP);
	if (!cd->converter[IPC4_COPIER_GATEWAY_PIN]) {
		comp_err(dev, "failed to get converter for QEMU gateway, dir %d",
			 cd->direction);
		ret = -EINVAL;
		goto e_qemugtw;
	}

	if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
		cd->bsource_buffer = false;
		pipeline->source_comp = dev;
	} else {
		cd->bsource_buffer = true;
		pipeline->sink_comp = dev;
	}

	list_item_append(&qemugtw_data->item, &qemugtw_list_head);
	cd->qemugtw_data = qemugtw_data;
	cd->endpoint_num++;

	return 0;

e_qemugtw:
	mod_free(mod, qemugtw_data);
	return ret;
}

__cold void copier_qemugtw_free(struct processing_module *mod)
{
	struct copier_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	if (cd->qemugtw_data) {
		list_item_del(&cd->qemugtw_data->item);
		mod_free(mod, cd->qemugtw_data);
		cd->qemugtw_data = NULL;
	}
}
