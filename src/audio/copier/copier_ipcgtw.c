// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2023 Intel Corporation. All rights reserved.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/audio_buffer.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/trace/trace.h>
#include <sof/lib/memory.h>
#include <sof/ut.h>
#include <rtos/init.h>
#include "copier.h"
#include "ipcgtw_copier.h"

LOG_MODULE_REGISTER(ipcgtw, CONFIG_SOF_LOG_LEVEL);

/* List of existing IPC gateways */
static struct list_item ipcgtw_list_head = LIST_INIT(ipcgtw_list_head);

static struct comp_dev *find_ipcgtw_by_node_id(union ipc4_connector_node_id node_id)
{
	struct list_item *item;

	list_for_item(item, &ipcgtw_list_head) {
		struct ipcgtw_data *data = list_item(item, struct ipcgtw_data, item);

		if (data->node_id.dw == node_id.dw)
			return data->dev;
	}

	return NULL;
}

static int sink_copy_bytes_from_linear(const void *linear_source,
				       struct sof_sink *sink, size_t bytes)
{
	const uint8_t *src = (const uint8_t *)linear_source;
	uint8_t *snk, *snk_begin, *snk_end;
	size_t remaining = bytes;
	size_t snk_size;
	int ret;

	if (!bytes)
		return 0;

	ret = sink_get_buffer(sink, bytes, (void **)&snk, (void **)&snk_begin, &snk_size);
	if (ret)
		return ret;

	snk_end = snk_begin + snk_size;
	while (remaining) {
		size_t chunk = MIN(remaining, (size_t)cir_buf_bytes_without_wrap(snk, snk_end));

		ret = memcpy_s(snk, chunk, src, chunk);
		if (ret) {
			sink_commit_buffer(sink, bytes - remaining);
			return ret;
		}

		remaining -= chunk;
		src += chunk;
		snk = cir_buf_wrap(snk + chunk, snk_begin, snk_end);
	}

	return sink_commit_buffer(sink, bytes);
}

static int source_copy_bytes_to_linear(struct sof_source *source,
				       void *linear_sink, size_t bytes)
{
	const uint8_t *src, *src_begin, *src_end;
	uint8_t *snk = (uint8_t *)linear_sink;
	size_t remaining = bytes;
	size_t src_size;
	int ret;

	if (!bytes)
		return 0;

	ret = source_get_data(source, bytes, (const void **)&src,
			      (const void **)&src_begin, &src_size);
	if (ret)
		return ret;

	src_end = src_begin + src_size;
	while (remaining) {
		size_t chunk = MIN(remaining, (size_t)cir_buf_bytes_without_wrap(src, src_end));

		ret = memcpy_s(snk, chunk, src, chunk);
		if (ret) {
			source_release_data(source, bytes - remaining);
			return ret;
		}

		remaining -= chunk;
		snk += chunk;
		src = cir_buf_wrap(src + chunk, src_begin, src_end);
	}

	return source_release_data(source, bytes);
}

static inline struct sof_source *ipcgtw_get_source(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);

	return mod->num_of_sources ? mod->sources[0] : NULL;
}

static inline struct sof_sink *ipcgtw_get_sink(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);

	return mod->num_of_sinks ? mod->sinks[0] : NULL;
}

int copier_ipcgtw_process(const struct ipc4_ipcgtw_cmd *cmd,
			  void *reply_payload, uint32_t *reply_payload_size)
{
	const struct ipc4_ipc_gateway_cmd_data *in;
	struct ipc4_ipc_gateway_cmd_data_reply *out;
	struct sof_source *source = NULL;
	struct sof_sink *sink = NULL;
	struct comp_dev *dev;
	uint32_t data_size;
	int ret;

	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE,
				 sizeof(struct ipc4_ipc_gateway_cmd_data));
	in = (const struct ipc4_ipc_gateway_cmd_data *)MAILBOX_HOSTBOX_BASE;

	dev = find_ipcgtw_by_node_id(in->node_id);
	if (!dev)
		return -ENODEV;

	comp_dbg(dev, "%x %x",
		 cmd->primary.dat, cmd->extension.dat);

	out = (struct ipc4_ipc_gateway_cmd_data_reply *)reply_payload;

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		sink = ipcgtw_get_sink(dev);
	else
		source = ipcgtw_get_source(dev);

	/*
	 * NOTE: this func is called from the IPC processing task and can potentially be
	 * called before pipeline start, even before the buffer has been attached. In that
	 * case the sink/source handles are not available yet, so do not report an error but
	 * return 0 bytes available for GET_DATA and 0 bytes free for SET_DATA.
	 */
	if (!sink && !source)
		comp_warn(dev, "no buffer found");
	
	switch (cmd->primary.r.cmd) {
	case IPC4_IPCGWCMD_GET_DATA:
		if (source) {
			data_size = MIN(cmd->extension.r.data_size, SOF_IPC_MSG_MAX_SIZE - 4);
			data_size = MIN(data_size, source_get_data_available(source));
			ret = source_copy_bytes_to_linear(source, out->payload, data_size);
			if (ret)
				return ret;
			out->u.size_avail = source_get_data_available(source);
			*reply_payload_size = data_size + 4;
		} else {
			out->u.size_avail = 0;
			*reply_payload_size = 4;
		}
		break;

	case IPC4_IPCGWCMD_SET_DATA:
		if (sink) {
			data_size = MIN(cmd->extension.r.data_size,
					sink_get_free_size(sink));
			dcache_invalidate_region((__sparse_force void __sparse_cache *)
						 MAILBOX_HOSTBOX_BASE,
						 data_size +
						 offsetof(struct ipc4_ipc_gateway_cmd_data,
							  payload));
			ret = sink_copy_bytes_from_linear(in->payload, sink, data_size);
			if (ret)
				return ret;
			out->u.size_consumed = data_size;
			*reply_payload_size = 4;
		} else {
			out->u.size_consumed = 0;
			*reply_payload_size = 4;
		}
		break;

	case IPC4_IPCGWCMD_FLUSH_DATA:
		*reply_payload_size = 0;
		if (sink)
			audio_buffer_reset(sof_audio_buffer_from_sink(sink));
		else if (source)
			audio_buffer_reset(sof_audio_buffer_from_source(source));
		break;

	default:
		comp_err(dev, "unexpected cmd: %u",
			 (unsigned int)cmd->primary.r.cmd);
		return -EINVAL;
	}

	return 0;
}

int copier_ipcgtw_params(struct ipcgtw_data *ipcgtw_data, struct comp_dev *dev,
			 struct sof_ipc_stream_params *params)
{
	struct sof_sink *sink = NULL;
	struct sof_source *source = NULL;
	struct comp_buffer *buf;
	int err;

	comp_dbg(dev, "ipcgtw_params()");

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		sink = ipcgtw_get_sink(dev);
	else
		source = ipcgtw_get_source(dev);

	if (sink)
		buf = comp_buffer_get_from_sink(sink);
	else if (source)
		buf = comp_buffer_get_from_source(source);
	else {
		comp_err(dev, "no buffer found");
		return -EINVAL;
	}

	/* resize buffer to size specified in IPC gateway config blob */
	err = buffer_set_size(buf, ipcgtw_data->buf_size, 0);

	if (err < 0) {
		comp_err(dev, "failed to resize buffer to %u bytes",
			 ipcgtw_data->buf_size);
		return err;
	}

	return 0;
}

void copier_ipcgtw_reset(struct comp_dev *dev)
{
	struct sof_sink *sink = NULL;
	struct sof_source *source = NULL;

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		sink = ipcgtw_get_sink(dev);
	else
		source = ipcgtw_get_source(dev);

	if (sink)
		audio_buffer_reset(sof_audio_buffer_from_sink(sink));
	else if (source)
		audio_buffer_reset(sof_audio_buffer_from_source(source));
	else
		comp_warn(dev, "no buffer found");
}

__cold int copier_ipcgtw_create(struct processing_module *mod,
				const struct ipc4_copier_module_cfg *copier,
				struct pipeline *pipeline)
{
	struct copier_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_ipc_config *config = &dev->ipc_config;
	struct ipcgtw_data *ipcgtw_data;
	const struct ipc4_copier_gateway_cfg *gtw_cfg;
	const struct ipc4_ipc_gateway_config_blob *blob;
	int ret;

	assert_can_be_cold();

	gtw_cfg = &copier->gtw_cfg;
	if (!gtw_cfg->config_length) {
		comp_err(dev, "empty ipc4_gateway_config_data");
		return -EINVAL;
	}

	/* config_length is in dwords; require enough dwords to cover the
	 * gateway config header and the blob read below. Compare dword counts
	 * (rather than scaling config_length by 4) so a large host-supplied
	 * value cannot overflow the multiplication on 32-bit size_t.
	 */
	if (gtw_cfg->config_length <
	    SOF_DIV_ROUND_UP(sizeof(struct ipc4_gateway_config_data) +
			     sizeof(struct ipc4_ipc_gateway_config_blob),
			     sizeof(uint32_t))) {
		comp_err(dev, "ipc4_gateway_config_data too small: %u",
			 gtw_cfg->config_length);
		return -EINVAL;
	}

	cd->ipc_gtw = true;

	/* The IPC gateway is treated as a host gateway */
	config->type = SOF_COMP_HOST;
	cd->gtw_type = ipc4_gtw_host;

	ipcgtw_data = mod_zalloc(mod, sizeof(*ipcgtw_data));
	if (!ipcgtw_data)
		return -ENOMEM;

	ipcgtw_data->node_id = gtw_cfg->node_id;
	ipcgtw_data->dev = dev;

	blob = (const struct ipc4_ipc_gateway_config_blob *)
		((const struct ipc4_gateway_config_data *)gtw_cfg->config_data)->config_blob;

	/* The buffer connected to the IPC gateway will be resized later in ipcgtw_params()
	 * to the size specified in the IPC gateway blob.
	 */
	comp_dbg(dev, "buffer_size: %u", blob->buffer_size);
	ipcgtw_data->buf_size = blob->buffer_size;

	cd->converter[IPC4_COPIER_GATEWAY_PIN] =
		get_converter_func(&copier->base.audio_fmt,
				   &copier->out_fmt,
				   ipc4_gtw_host, IPC4_DIRECTION(cd->direction), DUMMY_CHMAP);
	if (!cd->converter[IPC4_COPIER_GATEWAY_PIN]) {
		comp_err(dev, "failed to get converter for IPC gateway, dir %d",
			 cd->direction);
		ret = -EINVAL;
		goto e_ipcgtw;
	}

	if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
		cd->bsource_buffer = false;
		pipeline->source_comp = dev;
	} else {
		cd->bsource_buffer = true;
		pipeline->sink_comp = dev;
	}

	list_item_append(&ipcgtw_data->item, &ipcgtw_list_head);
	cd->ipcgtw_data = ipcgtw_data;
	cd->endpoint_num++;

	return 0;

e_ipcgtw:
	mod_free(mod, ipcgtw_data);
	return ret;
}

__cold void copier_ipcgtw_free(struct processing_module *mod)
{
	struct copier_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	list_item_del(&cd->ipcgtw_data->item);
	mod_free(mod, cd->ipcgtw_data);
}
