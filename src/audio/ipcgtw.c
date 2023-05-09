// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2023 Intel Corporation. All rights reserved.

#include <sof/audio/component_ext.h>
#include <sof/trace/trace.h>
#include <sof/lib/uuid.h>
#include <sof/lib/memory.h>
#include <sof/ut.h>
#include <rtos/init.h>
#include <ipc4/ipcgtw.h>
#include <ipc4/copier.h>
#include <sof/audio/ipcgtw_copier.h>

static const struct comp_driver comp_ipcgtw;

LOG_MODULE_REGISTER(ipcgtw, CONFIG_SOF_LOG_LEVEL);

/* a814a1ca-0b83-466c-9587-2f35ff8d12e8 */
DECLARE_SOF_RT_UUID("ipcgw", ipcgtw_comp_uuid, 0xa814a1ca, 0x0b83, 0x466c,
		    0x95, 0x87, 0x2f, 0x35, 0xff, 0x8d, 0x12, 0xe8);

DECLARE_TR_CTX(ipcgtw_comp_tr, SOF_UUID(ipcgtw_comp_uuid), LOG_LEVEL_INFO);

/* List of existing IPC gateways */
static struct list_item ipcgtw_list_head = LIST_INIT(ipcgtw_list_head);

void ipcgtw_zephyr_new(struct ipcgtw_data *ipcgtw_data,
		       const struct ipc4_copier_gateway_cfg *gtw_cfg,
		       struct comp_dev *dev)
{
	const struct ipc4_ipc_gateway_config_blob *blob;

	ipcgtw_data->node_id = gtw_cfg->node_id;
	ipcgtw_data->dev = dev;

	blob = (const struct ipc4_ipc_gateway_config_blob *)
		((const struct ipc4_gateway_config_data *)gtw_cfg->config_data)->config_blob;

	/* Endpoint buffer is created in copier with size specified in copier config. That buffer
	 * will be resized to size specified in IPC gateway blob later in ipcgtw_params().
	 */
	comp_cl_dbg(&comp_ipcgtw, "ipcgtw_new(): buffer_size: %u", blob->buffer_size);
	ipcgtw_data->buf_size = blob->buffer_size;

	list_item_append(&ipcgtw_data->item, &ipcgtw_list_head);
}

static struct comp_dev *ipcgtw_new(const struct comp_driver *drv,
				   const struct comp_ipc_config *config,
				   const void *spec)
{
	struct comp_dev *dev;
	struct ipcgtw_data *ipcgtw_data;
	const struct ipc4_copier_gateway_cfg *gtw_cfg = spec;

	comp_cl_dbg(&comp_ipcgtw, "ipcgtw_new()");

	if (!gtw_cfg->config_length) {
		comp_cl_err(&comp_ipcgtw, "ipcgtw_new(): empty ipc4_gateway_config_data");
		return NULL;
	}

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	ipcgtw_data = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*ipcgtw_data));
	if (!ipcgtw_data) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, ipcgtw_data);

	ipcgtw_zephyr_new(ipcgtw_data, gtw_cfg, dev);

	dev->state = COMP_STATE_READY;
	return dev;
}

void ipcgtw_zephyr_free(struct ipcgtw_data *ipcgtw_data)
{
	list_item_del(&ipcgtw_data->item);
	rfree(ipcgtw_data);
}

static void ipcgtw_free(struct comp_dev *dev)
{
	struct ipcgtw_data *ipcgtw_data = comp_get_drvdata(dev);

	ipcgtw_zephyr_free(ipcgtw_data);
	rfree(dev);
}

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
						       struct audio_stream __sparse_cache *sink,
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
void audio_stream_copy_bytes_to_linear(const struct audio_stream __sparse_cache *source,
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

int ipcgtw_process_cmd(const struct ipc4_ipcgtw_cmd *cmd,
		       void *reply_payload, uint32_t *reply_payload_size)
{
	const struct ipc4_ipc_gateway_cmd_data *in;
	struct comp_dev *dev;
	struct comp_buffer *buf;
	struct comp_buffer __sparse_cache *buf_c;
	uint32_t data_size;
	struct ipc4_ipc_gateway_cmd_data_reply *out;

	comp_cl_dbg(&comp_ipcgtw, "ipcgtw_process_cmd(): %x %x",
		    cmd->primary.dat, cmd->extension.dat);

	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE,
				 sizeof(struct ipc4_ipc_gateway_cmd_data));
	in = (const struct ipc4_ipc_gateway_cmd_data *)MAILBOX_HOSTBOX_BASE;

	dev = find_ipcgtw_by_node_id(in->node_id);
	if (!dev) {
		comp_cl_err(&comp_ipcgtw, "ipcgtw_process_cmd(): node_id not found: %x",
			    in->node_id.dw);
		return -EINVAL;
	}

	buf = get_buffer(dev);

	if (buf) {
		buf_c = buffer_acquire(buf);
	} else {
		/* NOTE: this func is called from IPC processing task and can be potentially
		 * called before pipeline start even before buffer has been attached. In such
		 * case do not report error but return 0 bytes available for GET_DATA and
		 * 0 bytes free for SET_DATA.
		 */
		buf_c = NULL;
		comp_cl_warn(&comp_ipcgtw, "ipcgtw_process_cmd(): no buffer found");
	}

	out = (struct ipc4_ipc_gateway_cmd_data_reply *)reply_payload;

	switch (cmd->primary.r.cmd) {
	case IPC4_IPCGWCMD_GET_DATA:
		if (buf_c) {
			data_size = MIN(cmd->extension.r.data_size, SOF_IPC_MSG_MAX_SIZE - 4);
			data_size = MIN(data_size, audio_stream_get_avail_bytes(&buf_c->stream));
			buffer_stream_invalidate(buf_c, data_size);
			audio_stream_copy_bytes_to_linear(&buf_c->stream, out->payload, data_size);
			comp_update_buffer_consume(buf_c, data_size);
			out->u.size_avail = audio_stream_get_avail_bytes(&buf_c->stream);
			*reply_payload_size = data_size + 4;
		} else {
			out->u.size_avail = 0;
			*reply_payload_size = 4;
		}
		break;

	case IPC4_IPCGWCMD_SET_DATA:
		if (buf_c) {
			data_size = MIN(cmd->extension.r.data_size,
					audio_stream_get_free_bytes(&buf_c->stream));
			dcache_invalidate_region((__sparse_force void __sparse_cache *)
						 MAILBOX_HOSTBOX_BASE,
						 data_size +
						 offsetof(struct ipc4_ipc_gateway_cmd_data,
							  payload));
			audio_stream_copy_bytes_from_linear(in->payload, &buf_c->stream,
							    data_size);
			buffer_stream_writeback(buf_c, data_size);
			comp_update_buffer_produce(buf_c, data_size);
			out->u.size_consumed = data_size;
			*reply_payload_size = 4;
		} else {
			out->u.size_consumed = 0;
			*reply_payload_size = 4;
		}
		break;

	case IPC4_IPCGWCMD_FLUSH_DATA:
		*reply_payload_size = 0;
		if (buf_c)
			audio_stream_reset(&buf_c->stream);
		break;

	default:
		comp_cl_err(&comp_ipcgtw, "ipcgtw_process_cmd(): unexpected cmd: %u",
			    (unsigned int)cmd->primary.r.cmd);
		if (buf_c)
			buffer_release(buf_c);
		return -EINVAL;
	}

	if (buf_c)
		buffer_release(buf_c);
	return 0;
}

static int ipcgtw_copy(struct comp_dev *dev)
{
	return 0;
}

static int ipcgtw_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	struct comp_buffer *buf;
	struct comp_buffer __sparse_cache *buf_c;
	int err;
	struct ipcgtw_data *ipcgtw_data = comp_get_drvdata(dev);

	comp_cl_dbg(&comp_ipcgtw, "ipcgtw_params()");

	buf = get_buffer(dev);
	if (!buf) {
		comp_cl_err(&comp_ipcgtw, "ipcgtw_params(): no buffer found");
		return -EINVAL;
	}

	/* resize buffer to size specified in IPC gateway config blob */
	buf_c = buffer_acquire(buf);
	err = buffer_set_size(buf_c, ipcgtw_data->buf_size);
	buffer_release(buf_c);

	if (err < 0) {
		comp_cl_err(&comp_ipcgtw, "ipcgtw_params(): failed to resize buffer to %u bytes",
			    ipcgtw_data->buf_size);
		return err;
	}

	return 0;
}

static int ipcgtw_trigger(struct comp_dev *dev, int cmd)
{
	/* copier calls gateway ops.trigger() without checking if it is NULL or not,
	 * so this handler is needed mostly to prevent crash.
	 */
	int ret = comp_set_state(dev, cmd);

	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return 0;
}

static int ipcgtw_prepare(struct comp_dev *dev)
{
	/* copier calls gateway ops.prepare() without checking if it is NULL or not,
	 * so this handler is needed mostly to prevent crash.
	 */
	int ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);

	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return 0;
}

static int ipcgtw_reset(struct comp_dev *dev)
{
	struct comp_buffer *buf = get_buffer(dev);

	if (buf) {
		struct comp_buffer __sparse_cache *buf_c = buffer_acquire(buf);

		audio_stream_reset(&buf_c->stream);
		buffer_release(buf_c);
	} else {
		comp_cl_warn(&comp_ipcgtw, "ipcgtw_reset(): no buffer found");
	}

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

static const struct comp_driver comp_ipcgtw = {
	.uid  = SOF_RT_UUID(ipcgtw_comp_uuid),
	.tctx = &ipcgtw_comp_tr,
	.ops  = {
		.create  = ipcgtw_new,
		.free    = ipcgtw_free,
		.params  = ipcgtw_params,
		.trigger = ipcgtw_trigger,	/* only to prevent copier crash */
		.prepare = ipcgtw_prepare,	/* only to prevent copier crash */
		.reset   = ipcgtw_reset,
		.copy    = ipcgtw_copy,
	},
};

static SHARED_DATA struct comp_driver_info comp_ipcgtw_info = {
	.drv = &comp_ipcgtw,
};

UT_STATIC void sys_comp_ipcgtw_init(void)
{
	comp_register(platform_shared_get(&comp_ipcgtw_info, sizeof(comp_ipcgtw_info)));
}

DECLARE_MODULE(sys_comp_ipcgtw_init);
SOF_MODULE_INIT(ipcgtw, sys_comp_ipcgtw_init);
