// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2023 Intel Corporation. All rights reserved.

#include <sof/audio/component_ext.h>
#include <sof/trace/trace.h>
#include <sof/lib/uuid.h>
#include <sof/lib/memory.h>
#include <sof/ut.h>
#include <rtos/init.h>
#include <ipc4/copier.h>
#include <sof/audio/ipcgtw_copier.h>

LOG_MODULE_REGISTER(ipcgtw, CONFIG_SOF_LOG_LEVEL);

/* a814a1ca-0b83-466c-9587-2f35ff8d12e8 */
DECLARE_SOF_RT_UUID("ipcgw", ipcgtw_comp_uuid, 0xa814a1ca, 0x0b83, 0x466c,
		    0x95, 0x87, 0x2f, 0x35, 0xff, 0x8d, 0x12, 0xe8);

DECLARE_TR_CTX(ipcgtw_comp_tr, SOF_UUID(ipcgtw_comp_uuid), LOG_LEVEL_INFO);

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

static inline void audio_stream_copy_bytes_from_linear(const void *linear_source,
						       struct audio_stream *sink,
						       unsigned int bytes)
{
	const uint8_t *src = (const uint8_t *)linear_source;
	uint8_t *snk = audio_stream_wrap(sink, audio_stream_get_wptr(sink));
	size_t bytes_snk, bytes_copied;

	while (bytes) {
		bytes_snk = audio_stream_bytes_without_wrap(sink, snk);
		bytes_copied = MIN(bytes, bytes_snk);
		memcpy_s(snk, bytes_copied, src, bytes_copied);
		bytes -= bytes_copied;
		src += bytes_copied;
		snk = audio_stream_wrap(sink, snk + bytes_copied);
	}
}

static inline
void audio_stream_copy_bytes_to_linear(const struct audio_stream *source,
				       void *linear_sink, unsigned int bytes)
{
	uint8_t *src = audio_stream_wrap(source, audio_stream_get_rptr(source));
	uint8_t *snk = (uint8_t *)linear_sink;
	size_t bytes_src, bytes_copied;

	while (bytes) {
		bytes_src = audio_stream_bytes_without_wrap(source, src);
		bytes_copied = MIN(bytes, bytes_src);
		memcpy_s(snk, bytes_copied, src, bytes_copied);
		bytes -= bytes_copied;
		src = audio_stream_wrap(source, src + bytes_copied);
		snk += bytes_copied;
	}
}

static inline struct comp_buffer *get_buffer(struct comp_dev *dev)
{
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		if (list_is_empty(&dev->bsink_list))
			return NULL;
		return list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	}

	assert(dev->direction == SOF_IPC_STREAM_CAPTURE);

	if (list_is_empty(&dev->bsource_list))
		return NULL;
	return list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
}

int copier_ipcgtw_process(const struct ipc4_ipcgtw_cmd *cmd,
			  void *reply_payload, uint32_t *reply_payload_size)
{
	const struct ipc4_ipc_gateway_cmd_data *in;
	struct comp_dev *dev;
	struct comp_buffer *buf;
	uint32_t data_size;
	struct ipc4_ipc_gateway_cmd_data_reply *out;

	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE,
				 sizeof(struct ipc4_ipc_gateway_cmd_data));
	in = (const struct ipc4_ipc_gateway_cmd_data *)MAILBOX_HOSTBOX_BASE;

	dev = find_ipcgtw_by_node_id(in->node_id);
	if (!dev)
		return -ENODEV;

	comp_dbg(dev, "copier_ipcgtw_process(): %x %x",
		 cmd->primary.dat, cmd->extension.dat);

	buf = get_buffer(dev);

	if (!buf) {
		/* NOTE: this func is called from IPC processing task and can be potentially
		 * called before pipeline start even before buffer has been attached. In such
		 * case do not report error but return 0 bytes available for GET_DATA and
		 * 0 bytes free for SET_DATA.
		 */
		comp_warn(dev, "copier_ipcgtw_process(): no buffer found");
	}

	out = (struct ipc4_ipc_gateway_cmd_data_reply *)reply_payload;

	switch (cmd->primary.r.cmd) {
	case IPC4_IPCGWCMD_GET_DATA:
		if (buf) {
			data_size = MIN(cmd->extension.r.data_size, SOF_IPC_MSG_MAX_SIZE - 4);
			data_size = MIN(data_size, audio_stream_get_avail_bytes(&buf->stream));
			buffer_stream_invalidate(buf, data_size);
			audio_stream_copy_bytes_to_linear(&buf->stream, out->payload, data_size);
			comp_update_buffer_consume(buf, data_size);
			out->u.size_avail = audio_stream_get_avail_bytes(&buf->stream);
			*reply_payload_size = data_size + 4;
		} else {
			out->u.size_avail = 0;
			*reply_payload_size = 4;
		}
		break;

	case IPC4_IPCGWCMD_SET_DATA:
		if (buf) {
			data_size = MIN(cmd->extension.r.data_size,
					audio_stream_get_free_bytes(&buf->stream));
			dcache_invalidate_region((__sparse_force void __sparse_cache *)
						 MAILBOX_HOSTBOX_BASE,
						 data_size +
						 offsetof(struct ipc4_ipc_gateway_cmd_data,
							  payload));
			audio_stream_copy_bytes_from_linear(in->payload, &buf->stream,
							    data_size);
			buffer_stream_writeback(buf, data_size);
			comp_update_buffer_produce(buf, data_size);
			out->u.size_consumed = data_size;
			*reply_payload_size = 4;
		} else {
			out->u.size_consumed = 0;
			*reply_payload_size = 4;
		}
		break;

	case IPC4_IPCGWCMD_FLUSH_DATA:
		*reply_payload_size = 0;
		if (buf)
			audio_stream_reset(&buf->stream);
		break;

	default:
		comp_err(dev, "copier_ipcgtw_process(): unexpected cmd: %u",
			 (unsigned int)cmd->primary.r.cmd);
		return -EINVAL;
	}

	return 0;
}

int copier_ipcgtw_params(struct ipcgtw_data *ipcgtw_data, struct comp_dev *dev,
			 struct sof_ipc_stream_params *params)
{
	struct comp_buffer *buf;
	int err;

	comp_dbg(dev, "ipcgtw_params()");

	buf = get_buffer(dev);
	if (!buf) {
		comp_err(dev, "ipcgtw_params(): no buffer found");
		return -EINVAL;
	}

	/* resize buffer to size specified in IPC gateway config blob */
	err = buffer_set_size(buf, ipcgtw_data->buf_size, 0);

	if (err < 0) {
		comp_err(dev, "ipcgtw_params(): failed to resize buffer to %u bytes",
			 ipcgtw_data->buf_size);
		return err;
	}

	return 0;
}

void copier_ipcgtw_reset(struct comp_dev *dev)
{
	struct comp_buffer *buf = get_buffer(dev);

	if (buf) {
		audio_stream_reset(&buf->stream);
	} else {
		comp_warn(dev, "ipcgtw_reset(): no buffer found");
	}
}

int copier_ipcgtw_create(struct comp_dev *dev, struct copier_data *cd,
			 const struct ipc4_copier_module_cfg *copier, struct pipeline *pipeline)
{
	struct comp_ipc_config *config = &dev->ipc_config;
	struct ipcgtw_data *ipcgtw_data;
	const struct ipc4_copier_gateway_cfg *gtw_cfg;
	const struct ipc4_ipc_gateway_config_blob *blob;
	int ret;

	gtw_cfg = &copier->gtw_cfg;
	if (!gtw_cfg->config_length) {
		comp_err(dev, "ipcgtw_create(): empty ipc4_gateway_config_data");
		return -EINVAL;
	}

	cd->ipc_gtw = true;

	/* create_endpoint_buffer() uses this value to choose between input and
	 * output formats in copier config to setup buffer. For this purpose
	 * IPC gateway should be handled similarly as host gateway.
	 */
	config->type = SOF_COMP_HOST;

	ret = create_endpoint_buffer(dev, cd, copier, false);
	if (ret < 0)
		return ret;

	ipcgtw_data = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*ipcgtw_data));
	if (!ipcgtw_data) {
		ret = -ENOMEM;
		goto e_buf;
	}

	ipcgtw_data->node_id = gtw_cfg->node_id;
	ipcgtw_data->dev = dev;

	blob = (const struct ipc4_ipc_gateway_config_blob *)
		((const struct ipc4_gateway_config_data *)gtw_cfg->config_data)->config_blob;

	/* Endpoint buffer is created in copier with size specified in copier config. That buffer
	 * will be resized to size specified in IPC gateway blob later in ipcgtw_params().
	 */
	comp_dbg(dev, "ipcgtw_create(): buffer_size: %u", blob->buffer_size);
	ipcgtw_data->buf_size = blob->buffer_size;

	cd->converter[IPC4_COPIER_GATEWAY_PIN] =
		get_converter_func(&copier->base.audio_fmt,
				   &copier->out_fmt,
				   ipc4_gtw_host, IPC4_DIRECTION(cd->direction));
	if (!cd->converter[IPC4_COPIER_GATEWAY_PIN]) {
		comp_err(dev, "failed to get converter for IPC gateway, dir %d",
			 cd->direction);
		ret = -EINVAL;
		goto e_ipcgtw;
	}

	if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
		comp_buffer_connect(dev, config->core,
				    cd->endpoint_buffer[cd->endpoint_num],
				    PPL_CONN_DIR_COMP_TO_BUFFER);
		cd->bsource_buffer = false;
		pipeline->source_comp = dev;
	} else {
		comp_buffer_connect(dev, config->core,
				    cd->endpoint_buffer[cd->endpoint_num],
				    PPL_CONN_DIR_BUFFER_TO_COMP);
		cd->bsource_buffer = true;
		pipeline->sink_comp = dev;
	}

	list_item_append(&ipcgtw_data->item, &ipcgtw_list_head);
	cd->ipcgtw_data = ipcgtw_data;
	cd->endpoint_num++;

	return 0;

e_ipcgtw:
	rfree(ipcgtw_data);
e_buf:
	buffer_free(cd->endpoint_buffer[cd->endpoint_num]);
	return ret;
}

void copier_ipcgtw_free(struct copier_data *cd)
{
	list_item_del(&cd->ipcgtw_data->item);
	rfree(cd->ipcgtw_data);
	buffer_free(cd->endpoint_buffer[0]);
}
