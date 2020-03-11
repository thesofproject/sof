// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

/*
 * A key phrase buffer component.
 */

/**
 * \file audio/kpb.c
 * \brief Key phrase buffer component implementation
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/kpb.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/ipc.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/string.h>
#include <sof/ut.h>
#include <ipc/topology.h>
#include <user/kpb.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static const struct comp_driver comp_kpb;

/* d8218443-5ff3-4a4c-b388-6cfe07b9562e */
DECLARE_SOF_UUID("kpb", kpb_uuid, 0xd8218443, 0x5ff3, 0x4a4c,
		 0xb3, 0x88, 0x6c, 0xfe, 0x07, 0xb9, 0x56, 0x2e);

/* KPB private data, runtime data */
struct comp_data {
	uint64_t state_log; /**< keeps record of KPB recent states */
	enum kpb_state state; /**< current state of KPB component */
	uint32_t kpb_no_of_clients; /**< number of registered clients */
	struct kpb_client clients[KPB_MAX_NO_OF_CLIENTS];
	struct task draining_task;
	uint32_t source_period_bytes; /**< source number of period bytes */
	uint32_t sink_period_bytes; /**< sink number of period bytes */
	struct sof_kpb_config config;   /**< component configuration data */
	struct comp_buffer *sel_sink; /**< real time sink (channel selector )*/
	struct comp_buffer *host_sink; /**< draining sink (client) */
	struct hb *history_buffer; /**< internal history buffer */
	size_t buffered_data; /**< keeps info about amount of buffered data */
	struct dd draining_task_data;
	size_t buffer_size; /**< size of internal history buffer */
	size_t host_buffer_size; /**< size of host buffer */
	size_t host_period_size; /**< size of history period */
	bool sync_draining_mode; /**< should we synchronize draining with
				   * host?
				   */
	spinlock_t lock; /**< locking mechanism for read pointer calculations */
};

/*! KPB private functions */
static void kpb_event_handler(void *arg, enum notify_id type, void *event_data);
static int kpb_register_client(struct comp_data *kpb, struct kpb_client *cli);
static void kpb_init_draining(struct comp_dev *dev, struct kpb_client *cli);
static enum task_state kpb_draining_task(void *arg);
static int kpb_buffer_data(struct comp_dev *dev,
			   const struct comp_buffer *source, size_t size);
static size_t kpb_allocate_history_buffer(struct comp_data *kpb,
					  size_t hb_size_req);
static void kpb_clear_history_buffer(struct hb *buff);
static void kpb_free_history_buffer(struct hb *buff);
static inline bool kpb_is_sample_width_supported(uint32_t sampling_width);
static void kpb_copy_samples(struct comp_buffer *sink,
			     struct comp_buffer *source, size_t size,
			     size_t sample_width);
static void kpb_drain_samples(void *source, struct audio_stream *sink,
			      size_t size, size_t sample_width);
static void kpb_buffer_samples(const struct audio_stream *source,
			       uint32_t start, void *sink, size_t size,
			       size_t sample_width);
static void kpb_reset_history_buffer(struct hb *buff);
static inline bool validate_host_params(struct comp_dev *dev,
					size_t host_period_size,
					size_t host_buffer_size);
static inline void kpb_change_state(struct comp_data *kpb,
				    enum kpb_state state);

static uint64_t kpb_task_deadline(void *data)
{
	return SOF_TASK_DEADLINE_ALMOST_IDLE;
}

/**
 * \brief Create a key phrase buffer component.
 * \param[in] comp - generic ipc component pointer.
 *
 * \return: a pointer to newly created KPB component.
 */
static struct comp_dev *kpb_new(const struct comp_driver *drv,
				struct sof_ipc_comp *comp)
{
	struct sof_ipc_comp_process *ipc_process =
					(struct sof_ipc_comp_process *)comp;
	struct task_ops ops = {
		.run = kpb_draining_task,
		.get_deadline = kpb_task_deadline,
	};
	size_t bs = ipc_process->size;
	struct comp_dev *dev;
	struct comp_data *kpb;
	int ret;

	comp_cl_info(&comp_kpb, "kpb_new()");

	dev = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;
	dev->drv = drv;

	dev->size = COMP_SIZE(struct sof_ipc_comp_process);

	ret = memcpy_s(COMP_GET_IPC(dev, sof_ipc_comp_process),
		       sizeof(struct sof_ipc_comp_process),
		       comp, sizeof(struct sof_ipc_comp_process));
	assert(!ret);

	kpb = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*kpb));
	if (!kpb) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, kpb);

	ret = memcpy_s(&kpb->config, sizeof(kpb->config), ipc_process->data,
		       bs);
	assert(!ret);

	if (!kpb_is_sample_width_supported(kpb->config.sampling_width)) {
		comp_err(dev, "kpb_new() error: requested sampling width not supported");
		return NULL;
	}

	if (kpb->config.channels > KPB_MAX_SUPPORTED_CHANNELS) {
		comp_err(dev, "kpb_new() error: no of channels exceeded the limit");
		return NULL;
	}

	if (kpb->config.sampling_freq != KPB_SAMPLNG_FREQUENCY) {
		comp_err(dev, "kpb_new() error: requested sampling frequency not supported");
		return NULL;
	}

	/* Initialize draining task */
	schedule_task_init_edf(&kpb->draining_task, /* task structure */
			       &ops, /* task ops */
			       &kpb->draining_task_data, /* task private data */
			       0, /* core on which we should run */
			       0); /* no flags */

	/* Init basic component data */
	kpb->history_buffer = NULL;
	kpb->kpb_no_of_clients = 0;
	kpb->state_log = 0;

	/* Kpb has been created successfully */
	dev->state = COMP_STATE_READY;
	kpb_change_state(kpb, KPB_STATE_CREATED);

	return dev;
}

/**
 * \brief Allocate history buffer.
 * \param[in] kpb - KPB component data pointer.
 *
 * \return: none.
 */
static size_t kpb_allocate_history_buffer(struct comp_data *kpb,
					  size_t hb_size_req)
{
	struct hb *history_buffer;
	struct hb *new_hb = NULL;
	/*! Total allocation size */
	size_t hb_size = hb_size_req;
	/*! Current allocation size */
	size_t ca_size = hb_size;
	/*! Memory caps priorites for history buffer */
	int hb_mcp[KPB_NO_OF_MEM_POOLS] = {SOF_MEM_CAPS_LP, SOF_MEM_CAPS_HP,
					   SOF_MEM_CAPS_RAM };
	void *new_mem_block = NULL;
	size_t temp_ca_size;
	int i = 0;
	size_t allocated_size = 0;

	comp_cl_info(&comp_kpb, "kpb_allocate_history_buffer()");

	/* Initialize history buffer */
	kpb->history_buffer = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				      sizeof(struct hb));
	if (!kpb->history_buffer)
		return 0;
	kpb->history_buffer->next = kpb->history_buffer;
	kpb->history_buffer->prev = kpb->history_buffer;
	history_buffer = kpb->history_buffer;

	/* Allocate history buffer/s. KPB history buffer has a size of
	 * KPB_MAX_BUFFER_SIZE, since there is no single memory block
	 * that big, we need to allocate couple smaller blocks which
	 * linked together will form history buffer.
	 */
	while (hb_size > 0 && i < ARRAY_SIZE(hb_mcp)) {
		/* Try to allocate ca_size (current allocation size). At first
		 * attempt it will be equal to hb_size (history buffer size).
		 */
		new_mem_block = rballoc(0, hb_mcp[i], ca_size);

		if (new_mem_block) {
			/* We managed to allocate a block of ca_size.
			 * Now we initialize it.
			 */
			comp_cl_info(&comp_kpb, "kpb new memory block: %d",
				     ca_size);
			allocated_size += ca_size;
			history_buffer->start_addr = new_mem_block;
			history_buffer->end_addr = (char *)new_mem_block +
				ca_size;
			history_buffer->w_ptr = new_mem_block;
			history_buffer->r_ptr = new_mem_block;
			history_buffer->state = KPB_BUFFER_FREE;
			hb_size -= ca_size;
			history_buffer->next = kpb->history_buffer;
			/* Do we need another buffer? */
			if (hb_size > 0) {
				/* Yes, we still need at least one more buffer.
				 * Let's first create new container for it.
				 */
				new_hb = rzalloc(SOF_MEM_ZONE_RUNTIME, 0,
						 SOF_MEM_CAPS_RAM,
						 sizeof(struct hb));
				if (!new_hb)
					return 0;
				history_buffer->next = new_hb;
				new_hb->state = KPB_BUFFER_OFF;
				new_hb->prev = history_buffer;
				history_buffer = new_hb;
				kpb->history_buffer->prev = new_hb;
				ca_size = hb_size;
				i++;
			}
		} else {
			/* We've failed to allocate ca_size of that hb_mcp
			 * let's try again with some smaller size.
			 * NOTE! If we decrement by some small value,
			 * the allocation will take significant time.
			 * However, bigger values like
			 * HEAP_HP_BUFFER_BLOCK_SIZE will result in lower
			 * accuracy of allocation.
			 */
			temp_ca_size = ca_size - KPB_ALLOCATION_STEP;
			ca_size = (ca_size < temp_ca_size) ? 0 : temp_ca_size;
			if (ca_size == 0) {
				ca_size = hb_size;
				i++;
			}
			continue;
		}
	}

	comp_cl_info(&comp_kpb, "kpb_allocate_history_buffer(): allocated %d bytes",
		     allocated_size);

	return allocated_size;
}

/**
 * \brief Reclaim memory of a history buffer.
 * \param[in] buff - pointer to current history buffer.
 *
 * \return none.
 */
static void kpb_free_history_buffer(struct hb *buff)
{
	struct hb *_buff;
	struct hb *first_buff = buff;

	comp_cl_info(&comp_kpb, "kpb_free_history_buffer()");

	if (!buff)
		return;

	/* Free history buffer/s */
	do {
		/* First reclaim HB internal memory, then HB itself */
		if (buff->start_addr)
			rfree(buff->start_addr);

		_buff = buff->next;
		rfree(buff);
		buff = _buff;
	} while (buff != first_buff);
}

/**
 * \brief Reclaim memory of a key phrase buffer.
 * \param[in] dev - component device pointer.
 *
 * \return none.
 */
static void kpb_free(struct comp_dev *dev)
{
	struct comp_data *kpb = comp_get_drvdata(dev);

	comp_info(dev, "kpb_free()");

	/* Unregister KPB from notifications */
	notifier_unregister(dev, NULL, NOTIFIER_ID_KPB_CLIENT_EVT);

	/* Reclaim memory occupied by history buffer */
	kpb_free_history_buffer(kpb->history_buffer);
	kpb->history_buffer = NULL;
	kpb->buffer_size = 0;

	/* remove scheduling */
	schedule_task_free(&kpb->draining_task);

	/* Free KPB */
	rfree(kpb);
	rfree(dev);
}

/**
 * \brief Trigger a change of KPB state.
 * \param[in] dev - component device pointer.
 * \param[in] cmd - command type.
 * \return none.
 */
static int kpb_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "kpb_trigger()");

	return comp_set_state(dev, cmd);
}

static int kbp_verify_params(struct comp_dev *dev,
			     struct sof_ipc_stream_params *params)
{
	int ret;

	comp_dbg(dev, "kbp_verify_params()");

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "kpb_verify_params() error: comp_verify_params() failed");
		return ret;
	}

	return 0;
}

/**
 * \brief KPB params.
 * \param[in] dev - component device pointer.
 * \param[in] params - pcm params.
 * \return none.
 */
static int kpb_params(struct comp_dev *dev,
		      struct sof_ipc_stream_params *params)
{
	struct comp_data *kpb = comp_get_drvdata(dev);
	int err;

	if (dev->state == COMP_STATE_PREPARE) {
		comp_err(dev, "kpb_params(): kpb has been already configured.");
		return PPL_STATUS_PATH_STOP;
	}

	err = kbp_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "kpb_params(): pcm params verification failed");
		return -EINVAL;
	}

	kpb->host_buffer_size = params->buffer.size;
	kpb->host_period_size = params->host_period_bytes;
	kpb->config.sampling_width = params->sample_container_bytes * 8;

	return 0;
}

/**
 * \brief Prepare key phrase buffer.
 * \param[in] dev - kpb component device pointer.
 *
 * \return integer representing either:
 *	0 -> success
 *	-EINVAL -> failure.
 */
static int kpb_prepare(struct comp_dev *dev)
{
	struct comp_data *kpb = comp_get_drvdata(dev);
	int ret = 0;
	int i;
	struct list_item *blist;
	struct comp_buffer *sink;
	size_t hb_size_req = KPB_MAX_BUFFER_SIZE(kpb->config.sampling_width);

	comp_info(dev, "kpb_prepare()");

	if (kpb->state == KPB_STATE_RESETTING ||
	    kpb->state == KPB_STATE_RESET_FINISHING) {
		comp_cl_err(&comp_kpb, "kpb_prepare() error: can not prepare KPB due to ongoing reset, state log %x",
			    kpb->state_log);
		return -EBUSY;
	}

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	if (!validate_host_params(dev, kpb->host_period_size,
				  kpb->host_buffer_size)) {
		comp_cl_err(&comp_kpb, "kpb_prepare() error: wrong host params.");
		return -EINVAL;
	}

	kpb_change_state(kpb, KPB_STATE_PREPARING);

	/* Init private data */
	kpb->kpb_no_of_clients = 0;
	kpb->buffered_data = 0;
	kpb->sel_sink = NULL;
	kpb->host_sink = NULL;

	if (kpb->history_buffer && kpb->buffer_size < hb_size_req) {
		/* Host params has changed, we need to allocate new buffer */
		kpb_free_history_buffer(kpb->history_buffer);
		kpb->history_buffer = NULL;
	}

	if (!kpb->history_buffer) {
		/* Allocate history buffer */
		kpb->buffer_size = kpb_allocate_history_buffer(kpb,
							       hb_size_req);

		/* Have we allocated what we requested? */
		if (kpb->buffer_size < hb_size_req) {
			comp_cl_err(&comp_kpb, "kpb_prepare() error: failed to allocate space for KPB buffer");
			kpb_free_history_buffer(kpb->history_buffer);
			kpb->history_buffer = NULL;
			kpb->buffer_size = 0;
			return -EINVAL;
		}
	}
	/* Init history buffer */
	kpb_reset_history_buffer(kpb->history_buffer);

	/* Initialize clients data */
	for (i = 0; i < KPB_MAX_NO_OF_CLIENTS; i++) {
		kpb->clients[i].state = KPB_CLIENT_UNREGISTERED;
		kpb->clients[i].r_ptr = NULL;
	}

	/* Register KPB for notification */
	ret = notifier_register(dev, NULL, NOTIFIER_ID_KPB_CLIENT_EVT,
				kpb_event_handler);
	if (ret < 0) {
		kpb_free_history_buffer(kpb->history_buffer);
		kpb->history_buffer = NULL;
		return -ENOMEM;
	}

	/* Search for KPB related sinks.
	 * NOTE! We assume here that channel selector component device
	 * is connected to the KPB sinks as well as host device.
	 */
	list_for_item(blist, &dev->bsink_list) {
		sink = container_of(blist, struct comp_buffer, source_list);

		if (!sink->sink) {
			ret = -EINVAL;
			break;
		}
		if (dev_comp_type(sink->sink) == SOF_COMP_SELECTOR) {
			/* We found proper real time sink */
			kpb->sel_sink = sink;
		} else if (dev_comp_type(sink->sink) == SOF_COMP_HOST) {
			/* We found proper host sink */
			kpb->host_sink = sink;
		}
	}

	if (!kpb->sel_sink || !kpb->host_sink) {
		comp_info(dev, "kpb_prepare() error: could not find sinks: sel_sink %d host_sink %d",
			  (uint32_t)kpb->sel_sink, (uint32_t)kpb->host_sink);
		ret = -EIO;
	}

	/* Disallow sync_draining_mode for now */
	kpb->sync_draining_mode = false;

	kpb_change_state(kpb, KPB_STATE_RUN);

	return ret;
}

/**
 * \brief Used to pass standard and bespoke commands (with data) to component.
 * \param[in,out] dev - Volume base component device.
 * \param[in] cmd - Command type.
 * \param[in,out] data - Control command data.
 * \return Error code.
 */
static int kpb_cmd(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size)
{
	int ret = 0;

	return ret;
}

/**
 * \brief Resets KPB component.
 * \param[in,out] dev KPB base component device.
 * \return Error code.
 */
static int kpb_reset(struct comp_dev *dev)
{
	struct comp_data *kpb = comp_get_drvdata(dev);
	int ret = 0;

	comp_cl_info(&comp_kpb, "kpb_reset(): resetting from state %d, state log %x",
		     kpb->state, kpb->state_log);

	switch (kpb->state) {
	case KPB_STATE_BUFFERING:
	case KPB_STATE_DRAINING:
		/* KPB is performing some task now,
		 * terminate it gently.
		 */
		kpb_change_state(kpb, KPB_STATE_RESETTING);
		ret = -EBUSY;
		break;
	case KPB_STATE_DISABLED:
	case KPB_STATE_CREATED:
		/* Nothing to reset */
		ret = comp_set_state(dev, COMP_TRIGGER_RESET);
		break;
	default:
		kpb->buffered_data = 0;

		if (kpb->history_buffer) {
			/* Reset history buffer - zero its data, reset pointers
			 * and states.
			 */
			kpb_reset_history_buffer(kpb->history_buffer);
		}

		/* Unregister KPB from notifications */
		notifier_unregister(dev, NULL, NOTIFIER_ID_KPB_CLIENT_EVT);
		/* Finally KPB is ready after reset */
		kpb_change_state(kpb, KPB_STATE_PREPARING);

		ret = comp_set_state(dev, COMP_TRIGGER_RESET);
		break;
	}

	return ret;
}

/**
 * \brief Copy real time input stream into sink buffer,
 *	and in the same time buffers that input for
 *	later use by some of clients.
 *
 *\param[in] dev - kpb component device pointer.
 *
 * \return integer representing either:
 *	0 - success
 *	-EINVAL - failure.
 */
static int kpb_copy(struct comp_dev *dev)
{
	int ret = 0;
	struct comp_data *kpb = comp_get_drvdata(dev);
	struct comp_buffer *source = NULL;
	struct comp_buffer *sink = NULL;
	size_t copy_bytes = 0;
	size_t sample_width = kpb->config.sampling_width;
	uint32_t hb_free_space;
	uint32_t flags = 0;

	comp_dbg(dev, "kpb_copy()");

	/* Get source and sink buffers */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);

	buffer_lock(source, &flags);

	/* Validate source */
	if (!source || !source->stream.r_ptr) {
		comp_err(dev, "kpb_copy(): invalid source pointers.");
		ret = -EINVAL;
		buffer_unlock(source, flags);
		goto out;
	}

	buffer_unlock(source, flags);

	switch (kpb->state) {
	case KPB_STATE_RUN:
		/* In normal RUN state we simply copy to our sink. */
		sink = kpb->sel_sink;

		buffer_lock(sink, &flags);

		/* Validate sink */
		if (!sink || !sink->stream.w_ptr) {
			comp_err(dev, "kpb_copy(): invalid selector sink pointers.");
			ret = -EINVAL;
			buffer_unlock(sink, flags);
			goto out;
		}

		buffer_unlock(sink, flags);

		copy_bytes = MIN(sink->stream.free, source->stream.avail);
		if (!copy_bytes) {
			comp_err(dev, "kpb_copy() error: nothing to copy sink->free %d source->avail %d",
				 sink->stream.free, source->stream.avail);
			ret = PPL_STATUS_PATH_STOP;
			goto out;
		}

		kpb_copy_samples(sink, source, copy_bytes, sample_width);

		/* Buffer source data internally in history buffer for future
		 * use by clients.
		 */
		if (source->stream.avail <= kpb->buffer_size) {
			ret = kpb_buffer_data(dev, source, copy_bytes);
			if (ret) {
				comp_err(dev, "kpb_copy(): internal buffering failed.");
				goto out;
			}
			if (kpb->buffered_data < kpb->buffer_size) {
				hb_free_space = kpb->buffer_size -
					kpb->buffered_data;

				kpb->buffered_data +=
					MIN(copy_bytes, hb_free_space);
			}
		} else {
			comp_err(dev, "kpb_copy(): too much data to buffer.");
		}

		comp_update_buffer_produce(sink, copy_bytes);
		comp_update_buffer_consume(source, copy_bytes);

		ret = 0;
		break;
	case KPB_STATE_HOST_COPY:
		/* In host copy state we only copy to host buffer. */
		sink = kpb->host_sink;

		buffer_lock(sink, &flags);

		/* Validate sink */
		if (!sink || !sink->stream.w_ptr) {
			comp_err(dev, "kpb_copy(): invalid host sink pointers.");
			ret = -EINVAL;
			buffer_unlock(sink, flags);
			goto out;
		}

		buffer_unlock(sink, flags);

		copy_bytes = MIN(sink->stream.free, source->stream.avail);
		if (!copy_bytes) {
			comp_err(dev, "kpb_copy() error: nothing to copy sink->free %d source->avail %d",
				 sink->stream.free, source->stream.avail);
			ret = PPL_STATUS_PATH_STOP;
			goto out;
		}

		kpb_copy_samples(sink, source, copy_bytes, sample_width);

		comp_update_buffer_produce(sink, copy_bytes);
		comp_update_buffer_consume(source, copy_bytes);

		break;
	case KPB_STATE_INIT_DRAINING:
	case KPB_STATE_DRAINING:
		/* In draining and init draining we only buffer data in
		 * the internal history buffer.
		 */
		if (source->stream.avail <= kpb->buffer_size) {
			buffer_invalidate(source, source->stream.avail);
			ret = kpb_buffer_data(dev, source,
					      source->stream.avail);
			if (ret) {
				comp_err(dev, "kpb_copy(): internal buffering failed.");
				goto out;
			}

			comp_update_buffer_consume(source,
						   source->stream.avail);
		} else {
			comp_err(dev, "kpb_copy(): too much data to buffer.");
		}

		ret = PPL_STATUS_PATH_STOP;
		break;
	default:
		comp_cl_err(&comp_kpb, "kpb_copy(): wrong state (state %d, state log %x)",
			    kpb->state, kpb->state_log);
		ret = -EIO;
		break;
	}

out:
	return ret;
}

/**
 * \brief Buffer real time data stream in
 *	the internal buffer.
 *
 * \param[in] kpb - KPB component data pointer.
 * \param[in] source pointer to the buffer source.
 *
 */
static int kpb_buffer_data(struct comp_dev *dev,
			   const struct comp_buffer *source, size_t size)
{
	int ret = 0;
	size_t size_to_copy = size;
	size_t space_avail;
	struct comp_data *kpb = comp_get_drvdata(dev);
	struct hb *buff = kpb->history_buffer;
	uint32_t offset = 0;
	uint64_t timeout = 0;
	uint64_t current_time;
	enum kpb_state state_preserved = kpb->state;
	struct dd *draining_data = &kpb->draining_task_data;
	size_t sample_width = kpb->config.sampling_width;
	struct timer *timer = timer_get();

	comp_dbg(dev, "kpb_buffer_data()");

	/* We are allowed to buffer data in internal history buffer
	 * only in KPB_STATE_RUN, KPB_STATE_DRAINING or KPB_STATE_INIT_DRAINING
	 * states.
	 */
	if (kpb->state != KPB_STATE_RUN &&
	    kpb->state != KPB_STATE_DRAINING &&
	    kpb->state != KPB_STATE_INIT_DRAINING) {
		comp_err(dev, "kpb_buffer_data() error: wrong state! (current state %d, state log %x)",
			 kpb->state, kpb->state_log);
		return PPL_STATUS_PATH_STOP;
	}

	if (kpb->state == KPB_STATE_DRAINING)
		draining_data->buffered_while_draining += size_to_copy;

	kpb_change_state(kpb, KPB_STATE_BUFFERING);

	timeout = platform_timer_get(timer) +
		  clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1);
	/* Let's store audio stream data in internal history buffer */
	while (size_to_copy) {
		/* Reset was requested, it's time to stop buffering and finish
		 * KPB reset.
		 */
		if (kpb->state == KPB_STATE_RESETTING) {
			kpb_change_state(kpb, KPB_STATE_RESET_FINISHING);
			kpb_reset(dev);
			return PPL_STATUS_PATH_STOP;
		}

		/* Are we stuck in buffering? */
		current_time = platform_timer_get(timer);
		if (timeout < current_time) {
			comp_err(dev, "kpb_buffer_data(): timeout of %d [ms] (current state %d, state log %x)",
				 current_time - timeout, kpb->state,
				 kpb->state_log);
			return -ETIME;
		}

		/* Check how much space there is in current write buffer */
		space_avail = (uint32_t)buff->end_addr - (uint32_t)buff->w_ptr;

		if (size_to_copy > space_avail) {
			/* We have more data to copy than available space
			 * in this buffer, copy what's available and continue
			 * with next buffer.
			 */
			kpb_buffer_samples(&source->stream, offset, buff->w_ptr,
					   space_avail, sample_width);
			/* Update write pointer & requested copy size */
			buff->w_ptr = (char *)buff->w_ptr + space_avail;
			size_to_copy = size_to_copy - space_avail;
			/* Update read pointer's offset before continuing
			 * with next buffer.
			 */
			offset += space_avail;
		} else {
			/* Requested size is smaller or equal to the space
			 * available in this buffer. In this scenario simply
			 * copy what was requested.
			 */
			kpb_buffer_samples(&source->stream, offset, buff->w_ptr,
					   size_to_copy, sample_width);
			/* Update write pointer & requested copy size */
			buff->w_ptr = (char *)buff->w_ptr + size_to_copy;
			/* Reset requested copy size */
			size_to_copy = 0;
		}
		/* Have we filled whole buffer? */
		if (buff->w_ptr == buff->end_addr) {
			/* Reset write pointer back to the beginning
			 * of the buffer.
			 */
			buff->w_ptr = buff->start_addr;
			/* If we have more buffers use them */
			if (buff->next && buff->next != buff) {
				/* Mark current buffer FULL */
				buff->state = KPB_BUFFER_FULL;
				/* Use next buffer available on the list
				 * of buffers.
				 */
				buff = buff->next;
				/* Update also component container,
				 * so next time we enter buffering function
				 * we will know right away what is the current
				 * write buffer
				 */
				kpb->history_buffer = buff;
			}
			/* Mark buffer as FREE */
			buff->state = KPB_BUFFER_FREE;
		}
	}

	kpb_change_state(kpb, state_preserved);
	return ret;
}

/**
 * \brief Main event dispatcher.
 * \param[in] arg - KPB component internal data.
 * \param[in] type - notification type
 * \param[in] event_data - event specific data.
 * \return none.
 */
static void kpb_event_handler(void *arg, enum notify_id type, void *event_data)
{
	struct comp_dev *dev = arg;
	struct comp_data *kpb = comp_get_drvdata(dev);
	struct kpb_event_data *evd = event_data;
	struct kpb_client *cli = evd->client_data;

	comp_info(dev, "kpb_event_handler(): received event with ID: %d ",
		  evd->event_id);

	switch (evd->event_id) {
	case KPB_EVENT_REGISTER_CLIENT:
		kpb_register_client(kpb, cli);
		break;
	case KPB_EVENT_UNREGISTER_CLIENT:
		/*TODO*/
		break;
	case KPB_EVENT_BEGIN_DRAINING:
		kpb_init_draining(dev, cli);
		break;
	case KPB_EVENT_STOP_DRAINING:
		/*TODO*/
		break;
	default:
		comp_err(dev, "kpb_cmd() error: unsupported command");
		break;
	}
}

/**
 * \brief Register clients in the system.
 *
 * \param[in] dev - kpb device component pointer.
 * \param[in] cli - pointer to KPB client's data.
 *
 * \return integer representing either:
 *	0 - success
 *	-EINVAL - failure.
 */
static int kpb_register_client(struct comp_data *kpb, struct kpb_client *cli)
{
	int ret = 0;

	comp_cl_info(&comp_kpb, "kpb_register_client()");

	if (!cli) {
		comp_cl_err(&comp_kpb, "kpb_register_client() error: no client data");
		return -EINVAL;
	}
	/* Do we have a room for a new client? */
	if (kpb->kpb_no_of_clients >= KPB_MAX_NO_OF_CLIENTS ||
	    cli->id >= KPB_MAX_NO_OF_CLIENTS) {
		comp_cl_err(&comp_kpb, "kpb_register_client() error: no free room for client = %u ",
			    cli->id);
		ret = -EINVAL;
	} else if (kpb->clients[cli->id].state != KPB_CLIENT_UNREGISTERED) {
		comp_cl_err(&comp_kpb, "kpb_register_client() error: client = %u already registered",
			    cli->id);
		ret = -EINVAL;
	} else {
		/* Client accepted, let's store his data */
		kpb->clients[cli->id].id  = cli->id;
		kpb->clients[cli->id].history_depth = cli->history_depth;
		kpb->clients[cli->id].sink = cli->sink;
		kpb->clients[cli->id].r_ptr = NULL;
		kpb->clients[cli->id].state = KPB_CLIENT_BUFFERING;
		kpb->kpb_no_of_clients++;
		ret = 0;
	}

	return ret;
}

/**
 * \brief Prepare history buffer for draining.
 *
 * \param[in] kpb - kpb component data.
 * \param[in] cli - client's data.
 *
 */
static void kpb_init_draining(struct comp_dev *dev, struct kpb_client *cli)
{
	struct comp_data *kpb = comp_get_drvdata(dev);
	bool is_sink_ready = (kpb->host_sink->sink->state == COMP_STATE_ACTIVE);
	size_t sample_width = kpb->config.sampling_width;
	size_t history_depth = cli->history_depth * kpb->config.channels *
			       (kpb->config.sampling_freq / 1000) *
			       (KPB_SAMPLE_CONTAINER_SIZE(sample_width) / 8);
	struct hb *buff = kpb->history_buffer;
	struct hb *first_buff = buff;
	size_t buffered = 0;
	size_t local_buffered;
	enum comp_copy_type copy_type = COMP_COPY_NORMAL;
	size_t drain_interval;
	size_t host_period_size = kpb->host_period_size;
	size_t ticks_per_ms = clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1);
	size_t bytes_per_ms = KPB_SAMPLES_PER_MS *
			      (KPB_SAMPLE_CONTAINER_SIZE(sample_width) / 8) *
			      kpb->config.channels;
	size_t period_bytes_limit;
	uint32_t flags;

	comp_info(dev, "kpb_init_draining(): requested draining of %d [ms] from history buffer",
		  cli->history_depth);

	if (kpb->state != KPB_STATE_RUN) {
		comp_err(dev, "kpb_init_draining() error: wrong KPB state");
	} else if (cli->id > KPB_MAX_NO_OF_CLIENTS) {
		comp_err(dev, "kpb_init_draining() error: wrong client id");
	/* TODO: check also if client is registered */
	} else if (!is_sink_ready) {
		comp_err(dev, "kpb_init_draining() error: sink not ready for draining");
	} else if (kpb->buffered_data < history_depth ||
		   kpb->buffer_size < history_depth) {
		comp_cl_err(&comp_kpb, "kpb_init_draining() error: not enough data in history buffer");
	} else {
		/* Draining accepted, find proper buffer to start reading
		 * At this point we are guaranteed that there is enough data
		 * in the history buffer. All we have to do now is to calculate
		 * read pointer from which we will start draining.
		 */
		spin_lock_irq(&kpb->lock, flags);

		kpb_change_state(kpb, KPB_STATE_INIT_DRAINING);

		do {
			/* Calculate how much data we have stored in
			 * current buffer.
			 */
			buff->r_ptr = buff->start_addr;
			if (buff->state == KPB_BUFFER_FREE) {
				local_buffered = (uint32_t)buff->w_ptr -
						 (uint32_t)buff->start_addr;
				buffered += local_buffered;
			} else if (buff->state == KPB_BUFFER_FULL) {
				local_buffered = (uint32_t)buff->end_addr -
						 (uint32_t)buff->start_addr;
				buffered += local_buffered;
			} else {
				comp_err(dev, "kpb_init_draining() error: incorrect buffer label");
			}
			/* Check if this is already sufficient to start draining
			 * if not, go to previous buffer and continue
			 * calculations.
			 */
			if (history_depth > buffered) {
				if (buff->prev == first_buff) {
					/* We went full circle and still don't
					 * have sufficient data for draining.
					 * That means we need to look up the
					 * first buffer again. Our read pointer
					 * is somewhere between write pointer
					 * and buffer's end address.
					 */
					buff = buff->prev;
					buffered += (uint32_t)buff->end_addr -
						    (uint32_t)buff->w_ptr;
					buff->r_ptr = (char *)buff->w_ptr +
						      (buffered - history_depth);
					break;
				}
				buff = buff->prev;
			} else if (history_depth == buffered) {
				buff->r_ptr = buff->start_addr;
				break;
			} else {
				buff->r_ptr = (char *)buff->start_addr +
					      (buffered - history_depth);
				break;
			}

		} while (buff != first_buff);
		spin_unlock_irq(&kpb->lock, flags);

		/* Should we drain in synchronized mode (sync_draining_mode)?
		 * Note! We have already verified host params during
		 * kpb_prepare().
		 */
		if (kpb->sync_draining_mode) {
			/* Calculate time in clock ticks each draining event
			 * shall take place. This time will be used to
			 * synchronize us with application interrupts.
			 */
			drain_interval = ((host_period_size / bytes_per_ms) *
					 ticks_per_ms) /
					 KPB_DRAIN_NUM_OF_PPL_PERIODS_AT_ONCE;
			period_bytes_limit = host_period_size;
			comp_info(dev, "kpb_init_draining(): sync_draining_mode selected with interval %d [uS].",
				  drain_interval * 1000 / ticks_per_ms);
		} else {
			/* Unlimited draining */
			drain_interval = 0;
			period_bytes_limit = 0;
			comp_info(dev, "kpb_init_draining: unlimited draining speed selected.");
		}

		comp_info(dev, "kpb_init_draining(), schedule draining task");

		/* Add one-time draining task into the scheduler. */
		kpb->draining_task_data.sink = kpb->host_sink;
		kpb->draining_task_data.history_buffer = buff;
		kpb->draining_task_data.history_depth = history_depth;
		kpb->draining_task_data.sample_width = sample_width;
		kpb->draining_task_data.drain_interval = drain_interval;
		kpb->draining_task_data.pb_limit = period_bytes_limit;
		kpb->draining_task_data.dev = dev;
		kpb->draining_task_data.sync_mode_on = kpb->sync_draining_mode;

		/* Set host-sink copy mode to blocking */
		comp_set_attribute(kpb->host_sink->sink, COMP_ATTR_COPY_TYPE,
				   &copy_type);

		/* Pause selector copy. */
		kpb->sel_sink->sink->state = COMP_STATE_PAUSED;

		/* Schedule draining task */
		schedule_task(&kpb->draining_task, 0, 0);
	}
}

/**
 * \brief Draining task.
 *
 * \param[in] arg - pointer keeping drainig data previously prepared
 * by kpb_init_draining().
 *
 * \return none.
 */
static enum task_state kpb_draining_task(void *arg)
{
	struct dd *draining_data = (struct dd *)arg;
	struct comp_buffer *sink = draining_data->sink;
	struct hb *buff = draining_data->history_buffer;
	size_t history_depth = draining_data->history_depth;
	size_t sample_width = draining_data->sample_width;
	size_t size_to_read;
	size_t size_to_copy;
	bool move_buffer = false;
	uint32_t drained = 0;
	uint64_t draining_time_start = 0;
	uint64_t draining_time_end = 0;
	enum comp_copy_type copy_type = COMP_COPY_NORMAL;
	uint64_t drain_interval = draining_data->drain_interval;
	uint64_t next_copy_time = 0;
	uint64_t current_time = 0;
	size_t period_bytes = 0;
	size_t period_bytes_limit = draining_data->pb_limit;
	struct timer *timer = timer_get();
	size_t period_copy_start = platform_timer_get(timer);
	size_t time_taken = 0;
	size_t *rt_stream_update = &draining_data->buffered_while_draining;
	struct comp_data *kpb = comp_get_drvdata(draining_data->dev);
	bool sync_mode_on = &draining_data->sync_mode_on;

	comp_cl_info(&comp_kpb, "kpb_draining_task(), start.");

	pm_runtime_disable(PM_RUNTIME_DSP, PLATFORM_MASTER_CORE_ID);

	/* Change KPB internal state to DRAINING */
	kpb_change_state(kpb, KPB_STATE_DRAINING);

	draining_time_start = platform_timer_get(timer);

	while (history_depth > 0) {
		/* Have we received reset request? */
		if (kpb->state == KPB_STATE_RESETTING) {
			kpb_change_state(kpb, KPB_STATE_RESET_FINISHING);
			kpb_reset(draining_data->dev);
			goto out;
		}
		/* Are we ready to drain further or host still need some time
		 * to read the data already provided?
		 */
		if (sync_mode_on &&
		    next_copy_time > platform_timer_get(timer)) {
			period_bytes = 0;
			period_copy_start = platform_timer_get(timer);
			continue;
		} else if (next_copy_time == 0) {
			period_copy_start = platform_timer_get(timer);
		}

		size_to_read = (uint32_t)buff->end_addr - (uint32_t)buff->r_ptr;

		if (size_to_read > sink->stream.free) {
			if (sink->stream.free >= history_depth)
				size_to_copy = history_depth;
			else
				size_to_copy = sink->stream.free;
		} else {
			if (size_to_read > history_depth) {
				size_to_copy = history_depth;
			} else {
				size_to_copy = size_to_read;
				move_buffer = true;
			}
		}

		kpb_drain_samples(buff->r_ptr, &sink->stream, size_to_copy,
				  sample_width);

		buff->r_ptr = (char *)buff->r_ptr + (uint32_t)size_to_copy;
		history_depth -= size_to_copy;
		drained += size_to_copy;
		period_bytes += size_to_copy;

		if (move_buffer) {
			buff->r_ptr = buff->start_addr;
			buff = buff->next;
			move_buffer = false;
		}

		if (size_to_copy) {
			comp_update_buffer_produce(sink, size_to_copy);
			comp_copy(sink->sink);
		}

		if (sync_mode_on && period_bytes >= period_bytes_limit) {
			current_time = platform_timer_get(timer);
			time_taken = current_time - period_copy_start;
			next_copy_time = current_time + drain_interval -
					 time_taken;
		}

		if (history_depth == 0) {
		/* We have finished draining of requested data however
		 * while we were draining real time stream could provided
		 * new data which needs to be copy to host.
		 */
			comp_cl_info(&comp_kpb, "kpb: update history_depth by %d",
				     *rt_stream_update);
			history_depth += *rt_stream_update;
			*rt_stream_update = 0;
		}
	}
out:
	draining_time_end = platform_timer_get(timer);

	/* Draining is done. Now switch KPB to copy real time stream
	 * to client's sink. This state is called "draining on demand"
	 * Note! If KPB state changed during draining due to i.e reset request
	 * we should not change that state.
	 */
	if (kpb->state == KPB_STATE_DRAINING)
		kpb_change_state(kpb, KPB_STATE_HOST_COPY);

	/* Reset host-sink copy mode back to unblocking */
	comp_set_attribute(sink->sink, COMP_ATTR_COPY_TYPE, &copy_type);

	comp_cl_info(&comp_kpb, "KPB: kpb_draining_task(), done. %u drained in %d ms",
		     drained,
		     (draining_time_end - draining_time_start)
		     / clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1));

	/* If traces are disabled, prevent compile error from unused
	 * variables.
	 */
	(void)(draining_time_end - draining_time_start);

	pm_runtime_enable(PM_RUNTIME_DSP, PLATFORM_MASTER_CORE_ID);

	return SOF_TASK_STATE_COMPLETED;
}

/**
 * \brief Drain data samples safe, according to configuration.
 *
 * \param[in] dev - kpb component device pointer
 * \param[in] sink - pointer to sink buffer.
 * \param[in] source - pointer to source buffer.
 * \param[in] size - requested copy size in bytes.
 *
 * \return none.
 */
static void kpb_drain_samples(void *source, struct audio_stream *sink,
			      size_t size, size_t sample_width)
{
#if CONFIG_FORMAT_S16LE || CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
	void *dst;
	void *src = source;
#endif
	size_t i;
	size_t j = 0;
	size_t channel;
	size_t frames = KPB_BYTES_TO_FRAMES(size, sample_width);

	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < KPB_NUM_OF_CHANNELS; channel++) {
			switch (sample_width) {
#if CONFIG_FORMAT_S16LE
			case 16:
				dst = audio_stream_write_frag_s16(sink, j);
				*((int16_t *)dst) = *((int16_t *)src);
				src = ((int16_t *)src) + 1;
				break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
			case 24:
			case 32:
				dst = audio_stream_write_frag_s32(sink, j);
				*((int32_t *)dst) = *((int32_t *)src);
				src = ((int32_t *)src) + 1;
				break;
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */
			default:
				comp_cl_err(&comp_kpb, "KPB: An attempt to copy not supported format!");
				return;
			}
			j++;
		}
	}
}

/**
 * \brief Buffers data samples safe, according to configuration.
 * \param[in,out] source Pointer to source buffer.
 * \param[in] start Start offset of source buffer in bytes.
 * \param[in,out] sink Pointer to sink buffer.
 * \param[in] size Requested copy size in bytes.
 * \param[in] sample_width Sample size.
 */
static void kpb_buffer_samples(const struct audio_stream *source,
			       uint32_t start, void *sink, size_t size,
			       size_t sample_width)
{
	void *src;
	void *dst = sink;
	size_t i;
	size_t j = start /
		(sample_width == 16 ? sizeof(int16_t) : sizeof(int32_t));
	size_t channel;
	size_t frames = KPB_BYTES_TO_FRAMES(size, sample_width);

	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < KPB_NUM_OF_CHANNELS; channel++) {
			switch (sample_width) {
			case 16:
				src = audio_stream_read_frag_s16(source, j);
				*((int16_t *)dst) = *((int16_t *)src);
				dst = ((int16_t *)dst) + 1;
				break;
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
			case 24:
			case 32:
				src = audio_stream_read_frag_s32(source, j);
				*((int32_t *)dst) = *((int32_t *)src);
				dst = ((int32_t *)dst) + 1;
				break;
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE*/
			default:
				comp_cl_err(&comp_kpb, "KPB: An attempt to copy not supported format!");
				return;
			}
			j++;
		}
	}
}

/**
 * \brief Initialize history buffer by zeroing its memory.
 * \param[in] buff - pointer to current history buffer.
 *
 * \return: none.
 */
static void kpb_clear_history_buffer(struct hb *buff)
{
	struct hb *first_buff = buff;
	void *start_addr;
	size_t size;

	comp_cl_info(&comp_kpb, "kpb_clear_history_buffer()");

	do {
		start_addr = buff->start_addr;
		size = (uint32_t)buff->end_addr - (uint32_t)start_addr;

		bzero(start_addr, size);

		buff = buff->next;
	} while (buff != first_buff);
}

static inline bool kpb_is_sample_width_supported(uint32_t sampling_width)
{
	bool ret;

	switch (sampling_width) {
#if CONFIG_FORMAT_S16LE
	case 16:
	/* FALLTHROUGH */
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case 24:
	/* FALLTHROUGH */
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case 32:
#endif /* CONFIG_FORMAT_S32LE */
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

/**
 * \brief Copy data samples safe, according to configuration.
 *
 * \param[in] dev - kpb component device pointer
 * \param[in] sink - pointer to sink buffer.
 * \param[in] source - pointer to source buffer.
 * \param[in] size - requested copy size in bytes.
 *
 * \return none.
 */
static void kpb_copy_samples(struct comp_buffer *sink,
			     struct comp_buffer *source, size_t size,
			     size_t sample_width)
{
#if CONFIG_FORMAT_S16LE || CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
	void *dst;
	void *src;
#endif
	size_t i;
	size_t j = 0;
	size_t channel;
	size_t frames = KPB_BYTES_TO_FRAMES(size, sample_width);
	struct audio_stream *istream = &source->stream;
	struct audio_stream *ostream = &sink->stream;

	buffer_invalidate(source, size);

	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < KPB_NUM_OF_CHANNELS; channel++) {
			switch (sample_width) {
#if CONFIG_FORMAT_S16LE
			case 16:
				dst = audio_stream_write_frag_s16(ostream, j);
				src = audio_stream_read_frag_s16(istream, j);
				*((int16_t *)dst) = *((int16_t *)src);
				break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
			case 24:
				/* FALLTHROUGH */
			case 32:
				dst = audio_stream_write_frag_s32(ostream, j);
				src = audio_stream_read_frag_s32(istream, j);
				*((int32_t *)dst) = *((int32_t *)src);
				break;
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE*/
			default:
				comp_cl_err(&comp_kpb, "KPB: An attempt to copy not supported format!");
				return;
			}
			j++;
		}
	}

	buffer_writeback(sink, size);
}

/**
 * \brief Reset history buffer.
 * \param[in] buff - pointer to current history buffer.
 *
 * \return none.
 */
static void kpb_reset_history_buffer(struct hb *buff)
{
	struct hb *first_buff = buff;

	comp_cl_info(&comp_kpb, "kpb_reset_history_buffer()");

	if (!buff)
		return;

	kpb_clear_history_buffer(buff);

	do {
		buff->w_ptr = buff->start_addr;
		buff->r_ptr = buff->start_addr;
		buff->state = KPB_BUFFER_FREE;

		buff = buff->next;

	} while (buff != first_buff);
}

static inline bool validate_host_params(struct comp_dev *dev,
					size_t host_period_size,
					size_t host_buffer_size)
{
	/* The aim of this function is to perform basic check of host params
	 * and reject them if they won't allow for stable draining.
	 * Note however that this is highly recommended for host buffer to
	 * be at least twice the history buffer size. This will quarantee
	 * "safe" draining.
	 * By safe we mean no XRUNs(host was unable to read data on time),
	 * or loss of data due to host delayed read. The later condition
	 * is very likely after wake up from power state like d0ix.
	 */
	struct comp_data *kpb = comp_get_drvdata(dev);
	size_t sample_width = kpb->config.sampling_width;
	size_t bytes_per_ms = KPB_SAMPLES_PER_MS *
			      (KPB_SAMPLE_CONTAINER_SIZE(sample_width) / 8) *
			      kpb->config.channels;
	size_t pipeline_period_size = (dev->pipeline->ipc_pipe.period / 1000)
					* bytes_per_ms;

	if (!host_period_size || !host_buffer_size) {
		/* Wrong host params */
		return false;
	} else if (HOST_BUFFER_MIN_SIZE(kpb->buffer_size) >
		   host_buffer_size) {
		/* Host buffer size is too small - history data
		 * may get overwritten.
		 */
		return false;
	} else if (kpb->sync_draining_mode) {
		/* Sync draining allowed. Check if we can perform draining
		 * with current settings.
		 * In this mode we copy host period size to host
		 * (to avoid overwrite of buffered data by real time stream
		 * this period shall be bigger than pipeline period) and
		 * give host some time to read it. Therefore, in worst
		 * case scenario, we copy one period of real time data + some
		 * of buffered data.
		 */
		if ((host_period_size / KPB_DRAIN_NUM_OF_PPL_PERIODS_AT_ONCE) <
		    pipeline_period_size)
			return false;
	}

	return true;
}

/**
 * \brief Change KPB state and log this change internally.
 * \param[in] kpb - KPB component data pointer.
 * \param[in] state - current KPB state.
 *
 * \return none.
 */
static inline void kpb_change_state(struct comp_data *kpb,
				    enum kpb_state state)
{
	comp_cl_dbg(&comp_kpb, "kpb_change_state(): from %d to %d",
		    kpb->state, state);
	kpb->state = state;
	kpb->state_log = (kpb->state_log << 4) | state;
}

static const struct comp_driver comp_kpb = {
	.type = SOF_COMP_KPB,
	.uid = SOF_UUID(kpb_uuid),
	.ops = {
		.new = kpb_new,
		.free = kpb_free,
		.cmd = kpb_cmd,
		.trigger = kpb_trigger,
		.copy = kpb_copy,
		.prepare = kpb_prepare,
		.reset = kpb_reset,
		.params = kpb_params,
	},
};

static SHARED_DATA struct comp_driver_info comp_kpb_info = {
	.drv = &comp_kpb,
};

UT_STATIC void sys_comp_kpb_init(void)
{
	comp_register(platform_shared_get(&comp_kpb_info,
					  sizeof(comp_kpb_info)));
}

DECLARE_MODULE(sys_comp_kpb_init);
