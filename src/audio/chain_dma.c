// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Piotr Makaruk <piotr.makaruk@intel.com>
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <ipc/dai.h>
#include <ipc4/gateway.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <sof/lib/dma.h>
#include <ipc4/error_status.h>
#include <ipc4/module.h>
#include <ipc4/pipeline.h>
#include <sof/ut.h>
#include <zephyr/pm/policy.h>
#include <rtos/init.h>
#if CONFIG_IPC4_XRUN_NOTIFICATIONS_ENABLE
#include <ipc4/notification.h>
#include <sof/ipc/msg.h>
#include <ipc/header.h>
#endif

static const struct comp_driver comp_chain_dma;
static const uint32_t max_chain_number = DAI_NUM_HDA_OUT + DAI_NUM_HDA_IN;

LOG_MODULE_REGISTER(chain_dma, CONFIG_SOF_LOG_LEVEL);

/* 6a0a274f-27cc-4afb-a3e7-3444723f432e */
DECLARE_SOF_RT_UUID("chain_dma", chain_dma_uuid, 0x6a0a274f, 0x27cc, 0x4afb,
		    0xa3, 0xe7, 0x34, 0x44, 0x72, 0x3f, 0x43, 0x2e);
DECLARE_TR_CTX(chain_dma_tr, SOF_UUID(chain_dma_uuid), LOG_LEVEL_INFO);

/* chain dma component private data */
struct chain_dma_data {
	bool first_data_received;
	/* node id of host HD/A DMA */
	union ipc4_connector_node_id host_connector_node_id;
	/* node id of link HD/A DMA */
	union ipc4_connector_node_id link_connector_node_id;
	uint32_t *hw_buffer;
	struct task chain_task;
	enum sof_ipc_stream_direction stream_direction;
	/* container size in bytes */
	uint8_t cs;
#if CONFIG_IPC4_XRUN_NOTIFICATIONS_ENABLE
	bool xrun_notification_sent;
	struct ipc_msg *msg_xrun;
#endif

	/* local host DMA config */
	struct dma *dma_host;
	struct dma_chan_data *chan_host;
	struct dma_config z_config_host;
	struct dma_block_config dma_block_cfg_host;

	/* local link DMA config */
	struct dma *dma_link;
	struct dma_chan_data *chan_link;
	struct dma_config z_config_link;
	struct dma_block_config dma_block_cfg_link;

	struct comp_buffer *dma_buffer;
};

static int chain_host_start(struct comp_dev *dev)
{
	struct chain_dma_data *cd = comp_get_drvdata(dev);
	int err;

	err = dma_start(cd->chan_host->dma->z_dev, cd->chan_host->index);
	if (err < 0)
		return err;

	comp_info(dev, "chain_host_start(): dma_start() host chan_index = %u",
		  cd->chan_host->index);
	return 0;
}

static int chain_link_start(struct comp_dev *dev)
{
	struct chain_dma_data *cd = comp_get_drvdata(dev);
	int err;

	err = dma_start(cd->chan_link->dma->z_dev, cd->chan_link->index);
	if (err < 0)
		return err;

	comp_info(dev, "chain_link_start(): dma_start() link chan_index = %u",
		  cd->chan_link->index);
	return 0;
}

static int chain_link_stop(struct comp_dev *dev)
{
	struct chain_dma_data *cd = comp_get_drvdata(dev);
	int err;

	err = dma_stop(cd->chan_link->dma->z_dev, cd->chan_link->index);
	if (err < 0)
		return err;

	comp_info(dev, "chain_link_stop(): dma_stop() link chan_index = %u",
		  cd->chan_link->index);

	return 0;
}

static int chain_host_stop(struct comp_dev *dev)
{
	struct chain_dma_data *cd = comp_get_drvdata(dev);
	int err;

	err = dma_stop(cd->chan_host->dma->z_dev, cd->chan_host->index);
	if (err < 0)
		return err;

	comp_info(dev, "chain_host_stop(): dma_stop() host chan_index = %u",
		  cd->chan_host->index);

	return 0;
}

/* Get size of data, which was consumed by link */
static size_t chain_get_transferred_data_size(const uint32_t out_read_pos, const uint32_t in_read_pos,
				       const size_t buff_size)
{
	if (out_read_pos >= in_read_pos)
		return out_read_pos - in_read_pos;

	return buff_size - in_read_pos + out_read_pos;
}

#if CONFIG_IPC4_XRUN_NOTIFICATIONS_ENABLE
static void handle_xrun(struct chain_dma_data *cd)
{
	int ret;

	if (cd->link_connector_node_id.f.dma_type == ipc4_hda_link_output_class &&
	    !cd->xrun_notification_sent) {
		tr_warn(&chain_dma_tr, "handle_xrun(): underrun detected");
		xrun_notif_msg_init(cd->msg_xrun, cd->link_connector_node_id.dw,
				    SOF_IPC4_GATEWAY_UNDERRUN_DETECTED);
		ipc_msg_send(cd->msg_xrun, NULL, true);
		cd->xrun_notification_sent = true;
	} else if (cd->link_connector_node_id.f.dma_type == ipc4_hda_link_input_class &&
		   !cd->xrun_notification_sent) {
		tr_warn(&chain_dma_tr, "handle_xrun(): overrun detected");
		xrun_notif_msg_init(cd->msg_xrun, cd->link_connector_node_id.dw,
				    SOF_IPC4_GATEWAY_OVERRUN_DETECTED);
		ipc_msg_send(cd->msg_xrun, NULL, true);
		cd->xrun_notification_sent = true;
	} else {
		/* if xrun_notification_sent is already set, then it means that link was
		 * able to reach stability therefore next underrun/overrun should be reported.
		 */
		cd->xrun_notification_sent = false;
	}
}
#endif

static enum task_state chain_task_run(void *data)
{
	size_t link_avail_bytes, link_free_bytes, host_avail_bytes, host_free_bytes;
	struct chain_dma_data *cd = data;
	uint32_t link_read_pos, host_read_pos;
	struct dma_status stat;
	uint32_t link_type;
	int ret;

	/* Link DMA can return -EPIPE and current status if xrun occurs, then it is not critical
	 * and flow shall continue. Other error values will be treated as critical.
	 */
	ret = dma_get_status(cd->chan_link->dma->z_dev, cd->chan_link->index, &stat);
	switch (ret) {
	case 0:
		break;
	case -EPIPE:
		tr_warn(&chain_dma_tr, "chain_task_run(): dma_get_status() link xrun occurred,"
			" ret = %u", ret);
#if CONFIG_IPC4_XRUN_NOTIFICATIONS_ENABLE
		handle_xrun(cd);
#endif
		break;
	default:
		tr_err(&chain_dma_tr, "chain_task_run(): dma_get_status() error, ret = %u", ret);
		return SOF_TASK_STATE_COMPLETED;
	}

	link_avail_bytes = stat.pending_length;
	link_free_bytes = stat.free;
	link_read_pos = stat.read_position;

	/* Host DMA does not report xruns. All error values will be treated as critical. */
	ret = dma_get_status(cd->chan_host->dma->z_dev, cd->chan_host->index, &stat);
	if (ret < 0) {
		tr_err(&chain_dma_tr, "chain_task_run(): dma_get_status() error, ret = %u", ret);
		return SOF_TASK_STATE_COMPLETED;
	}

	host_avail_bytes = stat.pending_length;
	host_free_bytes = stat.free;
	host_read_pos = stat.read_position;

	link_type = cd->link_connector_node_id.f.dma_type;
	if (link_type == ipc4_hda_link_input_class) {
		/* CAPTURE:
		 * When chained Link Input with Host Input immediately start transmitting data
		 * to host. In this mode task will always stream to host as much data as possible
		 */
		const size_t increment = MIN(host_free_bytes, link_avail_bytes);

		ret = dma_reload(cd->chan_host->dma->z_dev, cd->chan_host->index, 0, 0, increment);
		if (ret < 0) {
			tr_err(&chain_dma_tr,
			       "chain_task_run(): dma_reload() host error, ret = %u", ret);
			return SOF_TASK_STATE_COMPLETED;
		}

		ret = dma_reload(cd->chan_link->dma->z_dev, cd->chan_link->index, 0, 0, increment);
		if (ret < 0) {
			tr_err(&chain_dma_tr,
			       "chain_task_run(): dma_reload() link error, ret = %u", ret);
			return SOF_TASK_STATE_COMPLETED;
		}
	} else {
		/* PLAYBACK:
		 * When chained Host Output with Link Output then wait for half buffer full. In this
		 * mode task will update read position based on transferred data size to avoid
		 * overwriting valid data and write position by half buffer size.
		 */
		const size_t buff_size = audio_stream_get_size(&cd->dma_buffer->stream);
		const size_t half_buff_size = buff_size / 2;

		if (!cd->first_data_received && host_avail_bytes > half_buff_size) {
			ret = dma_reload(cd->chan_link->dma->z_dev,
					 cd->chan_link->index, 0, 0,
					 half_buff_size);
			if (ret < 0) {
				tr_err(&chain_dma_tr,
				       "chain_task_run(): dma_reload() link error, ret = %u",
					ret);
				return SOF_TASK_STATE_COMPLETED;
			}
			cd->first_data_received = true;

		} else if (cd->first_data_received) {
			const size_t transferred =
				chain_get_transferred_data_size(link_read_pos,
								host_read_pos,
								buff_size);

			ret = dma_reload(cd->chan_host->dma->z_dev, cd->chan_host->index,
					 0, 0, transferred);
			if (ret < 0) {
				tr_err(&chain_dma_tr,
				       "chain_task_run(): dma_reload() host error, ret = %u", ret);
				return SOF_TASK_STATE_COMPLETED;
			}

			if (host_avail_bytes >= half_buff_size &&
			    link_free_bytes >= half_buff_size) {
				ret = dma_reload(cd->chan_link->dma->z_dev, cd->chan_link->index,
						 0, 0, half_buff_size);
				if (ret < 0) {
					tr_err(&chain_dma_tr, "chain_task_run(): dma_reload() "
					       "link error, ret = %u", ret);
					return SOF_TASK_STATE_COMPLETED;
				}
			}
		}
	}
	return SOF_TASK_STATE_RESCHEDULE;
}

static int chain_task_start(struct comp_dev *dev)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	struct chain_dma_data *cd = comp_get_drvdata(dev);
	k_spinlock_key_t key;
	int ret;

	comp_info(dev, "chain_task_start(), host_dma_id = 0x%08x", cd->host_connector_node_id.dw);

	key = k_spin_lock(&drivers->lock);
	switch (cd->chain_task.state) {
	case SOF_TASK_STATE_QUEUED:
		k_spin_unlock(&drivers->lock, key);
		return 0;
	case SOF_TASK_STATE_COMPLETED:
		break;
	case SOF_TASK_STATE_INIT:
		break;
	case SOF_TASK_STATE_FREE:
		break;
	default:
		comp_err(dev, "chain_task_start(), bad state transition");
		ret = -EINVAL;
		goto error;
	}

	if (cd->stream_direction == SOF_IPC_STREAM_PLAYBACK) {
		ret = chain_host_start(dev);
		if (ret)
			goto error;
		ret = chain_link_start(dev);
		if (ret) {
			chain_host_stop(dev);
			goto error;
		}
	} else {
		ret = chain_link_start(dev);
		if (ret)
			goto error;
		ret = chain_host_start(dev);
		if (ret) {
			chain_link_stop(dev);
			goto error;
		}
	}

	ret = schedule_task_init_ll(&cd->chain_task, SOF_UUID(chain_dma_uuid),
				    SOF_SCHEDULE_LL_TIMER, SOF_TASK_PRI_HIGH,
				    chain_task_run, cd, 0, 0);
	if (ret < 0) {
		comp_err(dev, "chain_task_start(), ll task initialization failed");
		goto error_task;
	}

	ret = schedule_task(&cd->chain_task, 0, 0);
	if (ret < 0) {
		comp_err(dev, "chain_task_start(), ll schedule task failed");
		schedule_task_free(&cd->chain_task);
		goto error_task;
	}

	pm_policy_state_lock_get(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);
	k_spin_unlock(&drivers->lock, key);

	return 0;

error_task:
	chain_host_stop(dev);
	chain_link_stop(dev);
error:
	k_spin_unlock(&drivers->lock, key);
	return ret;
}

static int chain_task_pause(struct comp_dev *dev)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	struct chain_dma_data *cd = comp_get_drvdata(dev);
	k_spinlock_key_t key;
	int ret, ret2;

	if (cd->chain_task.state == SOF_TASK_STATE_FREE)
		return 0;

	key = k_spin_lock(&drivers->lock);
	cd->first_data_received = false;
	if (cd->stream_direction == SOF_IPC_STREAM_PLAYBACK) {
		ret = chain_host_stop(dev);
		ret2 = chain_link_stop(dev);
	} else {
		ret = chain_link_stop(dev);
		ret2 = chain_host_stop(dev);
	}
	if (!ret)
		ret = ret2;

	k_spin_unlock(&drivers->lock, key);

	schedule_task_free(&cd->chain_task);
	pm_policy_state_lock_put(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);

	return ret;
}

static void chain_release(struct comp_dev *dev)
{
	struct chain_dma_data *cd = comp_get_drvdata(dev);

	dma_release_channel(cd->chan_host->dma->z_dev, cd->chan_host->index);
	dma_put(cd->dma_host);
	dma_release_channel(cd->chan_link->dma->z_dev, cd->chan_link->index);
	dma_put(cd->dma_link);

	if (cd->dma_buffer) {
		buffer_free(cd->dma_buffer);
		cd->dma_buffer = NULL;
	}
}

/* Retrieves host connector node id from dma id */
static int get_connector_node_id(uint32_t dma_id, bool host_type,
			  union ipc4_connector_node_id *connector_node_id)
{
	uint8_t type = host_type ? ipc4_hda_host_output_class : ipc4_hda_link_output_class;

	if (dma_id >= DAI_NUM_HDA_OUT) {
		type = host_type ? ipc4_hda_host_input_class : ipc4_hda_link_input_class;
		dma_id -= DAI_NUM_HDA_OUT;
		if (dma_id >= DAI_NUM_HDA_IN)
			return -EINVAL;
	}
	connector_node_id->dw = 0;
	connector_node_id->f.dma_type = type;
	connector_node_id->f.v_index = dma_id;

	return 0;
}

static int chain_init(struct comp_dev *dev, void *addr, size_t length)
{
	struct chain_dma_data *cd = comp_get_drvdata(dev);
	struct dma_block_config *dma_block_cfg_host = &cd->dma_block_cfg_host;
	struct dma_block_config *dma_block_cfg_link = &cd->dma_block_cfg_link;
	struct dma_config *dma_cfg_host = &cd->z_config_host;
	struct dma_config *dma_cfg_link = &cd->z_config_link;
	int channel;
	int err;

	memset(dma_cfg_host, 0, sizeof(*dma_cfg_host));
	memset(dma_block_cfg_host, 0, sizeof(*dma_block_cfg_host));
	dma_cfg_host->block_count = 1;
	dma_cfg_host->source_data_size = cd->cs;
	dma_cfg_host->dest_data_size = cd->cs;
	dma_cfg_host->head_block = dma_block_cfg_host;
	dma_block_cfg_host->block_size = length;

	memset(dma_cfg_link, 0, sizeof(*dma_cfg_link));
	memset(dma_block_cfg_link, 0, sizeof(*dma_block_cfg_link));
	dma_cfg_link->block_count = 1;
	dma_cfg_link->source_data_size = cd->cs;
	dma_cfg_link->dest_data_size = cd->cs;
	dma_cfg_link->head_block  = dma_block_cfg_link;
	dma_block_cfg_link->block_size = length;

	switch (cd->stream_direction) {
	case SOF_IPC_STREAM_PLAYBACK:
		dma_cfg_host->channel_direction = HOST_TO_MEMORY;
		dma_block_cfg_host->dest_address = (uint32_t)addr;
		dma_cfg_link->channel_direction = MEMORY_TO_PERIPHERAL;
		dma_block_cfg_link->source_address = (uint32_t)addr;
		break;
	case SOF_IPC_STREAM_CAPTURE:
		dma_cfg_host->channel_direction = MEMORY_TO_HOST;
		dma_block_cfg_host->source_address = (uint32_t)addr;
		dma_cfg_link->channel_direction = PERIPHERAL_TO_MEMORY;
		dma_block_cfg_link->dest_address = (uint32_t)addr;
		break;
	}

	/* get host DMA channel */
	channel = cd->host_connector_node_id.f.v_index;
	channel = dma_request_channel(cd->dma_host->z_dev, &channel);
	if (channel < 0) {
		comp_err(dev, "chain_init(): dma_request_channel() failed");
		return -EINVAL;
	}

	cd->chan_host = &cd->dma_host->chan[channel];

	err = dma_config(cd->dma_host->z_dev, cd->chan_host->index, dma_cfg_host);
	if (err < 0) {
		comp_err(dev, "chain_init(): dma_config() failed");
		goto error_host;
	}

	/* get link DMA channel */
	channel = cd->link_connector_node_id.f.v_index;
	channel = dma_request_channel(cd->dma_link->z_dev, &channel);
	if (channel < 0) {
		comp_err(dev, "chain_init(): dma_request_channel() failed");
		goto error_host;
	}

	cd->chan_link = &cd->dma_link->chan[channel];

	err = dma_config(cd->dma_link->z_dev, cd->chan_link->index, dma_cfg_link);
	if (err < 0) {
		comp_err(dev, "chain_init(): dma_config() failed");
		goto error_link;
	}
	return 0;

error_link:
	dma_release_channel(cd->dma_link->z_dev, cd->chan_link->index);
	cd->chan_link = NULL;
error_host:
	dma_release_channel(cd->dma_host->z_dev, cd->chan_host->index);
	cd->chan_host = NULL;
	return err;
}

static int chain_task_init(struct comp_dev *dev, uint8_t host_dma_id, uint8_t link_dma_id,
		    uint32_t fifo_size)
{
	struct chain_dma_data *cd = comp_get_drvdata(dev);
	uint32_t addr_align;
	size_t buff_size;
	void *buff_addr;
	uint32_t dir;
	int ret;

	ret = get_connector_node_id(host_dma_id, true, &cd->host_connector_node_id);
	if (ret < 0)
		return ret;

	ret = get_connector_node_id(link_dma_id, false, &cd->link_connector_node_id);
	if (ret < 0)
		return ret;

	/* Verify whether HDA gateways can be chained */
	if (cd->host_connector_node_id.f.dma_type == ipc4_hda_host_output_class) {
		if (cd->link_connector_node_id.f.dma_type != ipc4_hda_link_output_class)
			return -EINVAL;
		cd->stream_direction = SOF_IPC_STREAM_PLAYBACK;
	}
	if (cd->host_connector_node_id.f.dma_type == ipc4_hda_host_input_class) {
		if (cd->link_connector_node_id.f.dma_type != ipc4_hda_link_input_class)
			return -EINVAL;
		cd->stream_direction = SOF_IPC_STREAM_CAPTURE;
	}

	/* request HDA DMA with shared access privilege */
	dir = (cd->stream_direction == SOF_IPC_STREAM_PLAYBACK) ?
		DMA_DIR_HMEM_TO_LMEM : DMA_DIR_LMEM_TO_HMEM;

	cd->dma_host = dma_get(dir, 0, DMA_DEV_HOST, DMA_ACCESS_SHARED);
	if (!cd->dma_host) {
		comp_err(dev, "chain_task_init(): dma_get() returned NULL");
		return -EINVAL;
	}

	dir = (cd->stream_direction == SOF_IPC_STREAM_PLAYBACK) ?
		DMA_DIR_MEM_TO_DEV : DMA_DIR_DEV_TO_MEM;

	cd->dma_link = dma_get(dir, DMA_CAP_HDA, DMA_DEV_HDA, DMA_ACCESS_SHARED);
	if (!cd->dma_link) {
		dma_put(cd->dma_host);
		comp_err(dev, "chain_task_init(): dma_get() returned NULL");
		return -EINVAL;
	}

	/* retrieve DMA buffer address alignment */
	ret = dma_get_attribute(cd->dma_host->z_dev, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				&addr_align);
	if (ret < 0) {
		comp_err(dev,
			 "chain_task_init(): could not get dma buffer address alignment, err = %d",
			 ret);
		goto error;
	}

	switch (cd->link_connector_node_id.f.dma_type) {
	case ipc4_hda_link_input_class:
		/* Increasing buffer size for capture path as L1SEN exit takes sometimes
		 * more than expected. To prevent from glitches and DMA overruns buffer
		 * is increased 5 times.
		 */
		fifo_size *= 5;
		break;
	case ipc4_hda_link_output_class:
		/* Increasing buffer size for playback path as L1SEN exit takes sometimes
		 * more that expected
		 * Note, FIFO size must be smaller than half of host buffer size
		 * (20ms ping pong) to avoid problems with position reporting
		 * Size increase from default 2ms to 5ms is enough.
		 */
		fifo_size *= 5;
		fifo_size /= 2;
		break;
	}

	fifo_size = ALIGN_UP_INTERNAL(fifo_size, addr_align);
	/* allocate not shared buffer */
	cd->dma_buffer = buffer_alloc(fifo_size, SOF_MEM_CAPS_DMA, 0, addr_align, false);

	if (!cd->dma_buffer) {
		comp_err(dev, "chain_task_init(): failed to alloc dma buffer");
		ret = -EINVAL;
		goto error;
	}

	/* clear dma buffer */
	buffer_zero(cd->dma_buffer);
	buff_addr = audio_stream_get_addr(&cd->dma_buffer->stream);
	buff_size = audio_stream_get_size(&cd->dma_buffer->stream);

	ret = chain_init(dev, buff_addr, buff_size);
	if (ret < 0) {
		buffer_free(cd->dma_buffer);
		cd->dma_buffer = NULL;
		goto error;
	}

	cd->chain_task.state = SOF_TASK_STATE_INIT;

	return 0;
error:
	dma_put(cd->dma_host);
	dma_put(cd->dma_link);
	return ret;
}

static int chain_task_trigger(struct comp_dev *dev, int cmd)
{
	switch (cmd) {
	case COMP_TRIGGER_START:
		return chain_task_start(dev);
	case COMP_TRIGGER_PAUSE:
		return chain_task_pause(dev);
	default:
		return -EINVAL;
	}
}

static struct comp_dev *chain_task_create(const struct comp_driver *drv,
					  const struct comp_ipc_config *ipc_config,
					  const void *ipc_specific_config)
{
	const struct ipc4_chain_dma *cdma = (struct ipc4_chain_dma *)ipc_specific_config;
	const uint32_t host_dma_id = cdma->primary.r.host_dma_id;
	const uint32_t link_dma_id = cdma->primary.r.link_dma_id;
	const uint32_t fifo_size = cdma->extension.r.fifo_size;
	const bool scs = cdma->primary.r.scs;
	struct chain_dma_data *cd;
	struct comp_dev *dev;
	int ret;

	if (host_dma_id >= max_chain_number)
		return NULL;

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		goto error;

	cd->first_data_received = false;
	cd->cs = scs ? 2 : 4;
	cd->chain_task.state = SOF_TASK_STATE_INIT;

	comp_set_drvdata(dev, cd);

	ret = chain_task_init(dev, host_dma_id, link_dma_id, fifo_size);
	if (ret)
		goto error_cd;

#if CONFIG_IPC4_XRUN_NOTIFICATIONS_ENABLE
	cd->msg_xrun = ipc_msg_init(header.dat,
				    sizeof(struct ipc4_resource_event_data_notification));
	if (!cd->msg_xrun)
		goto error_cd;
	cd->xrun_notification_sent = false;
#endif

	return dev;

error_cd:
	rfree(cd);
error:
	rfree(dev);
	return NULL;
}

static void chain_task_free(struct comp_dev *dev)
{
	struct chain_dma_data *cd = comp_get_drvdata(dev);

	chain_release(dev);
	rfree(cd);
	rfree(dev);
}

static const struct comp_driver  comp_chain_dma = {
	.uid = SOF_RT_UUID(chain_dma_uuid),
	.tctx = &chain_dma_tr,
	.ops = {
		.create = chain_task_create,
		.trigger = chain_task_trigger,
		.free = chain_task_free,
	},
};

static SHARED_DATA struct comp_driver_info comp_chain_dma_info = {
	.drv = &comp_chain_dma,
};

UT_STATIC void sys_comp_chain_dma_init(void)
{
	comp_register(platform_shared_get(&comp_chain_dma_info,
					  sizeof(comp_chain_dma_info)));
}

DECLARE_MODULE(sys_comp_chain_dma_init);
SOF_MODULE_INIT(chain_dma, sys_comp_chain_dma_init);
