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
#include <sof/lib/notifier.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
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

/* KPB private data, runtime data */
struct comp_data {
	enum kpb_state state; /**< current state of KPB component */
	uint32_t kpb_no_of_clients; /**< number of registered clients */
	struct kpb_client clients[KPB_MAX_NO_OF_CLIENTS];
	struct notifier kpb_events; /**< KPB events object */
	struct task draining_task;
	uint32_t source_period_bytes; /**< source number of period bytes */
	uint32_t sink_period_bytes; /**< sink number of period bytes */
	struct sof_kpb_config config;   /**< component configuration data */
	struct comp_buffer *sel_sink; /**< real time sink (channel selector ) */
	struct comp_buffer *host_sink; /**< draining sink (client) */
	struct hb *history_buffer;
	bool is_internal_buffer_full;
	size_t buffered_data;
	struct dd draining_task_data;
	size_t kpb_buffer_size;
	size_t host_buffer_size;
	size_t host_period_size;
};

/*! KPB private functions */
static void kpb_event_handler(int message, void *cb_data, void *event_data);
static int kpb_register_client(struct comp_data *kpb, struct kpb_client *cli);
static void kpb_init_draining(struct comp_dev *dev, struct kpb_client *cli);
static uint64_t kpb_draining_task(void *arg);
static int kpb_buffer_data(struct comp_dev *dev, struct comp_buffer *source,
			   size_t size);
static size_t kpb_allocate_history_buffer(struct comp_data *kpb);
static void kpb_clear_history_buffer(struct hb *buff);
static void kpb_free_history_buffer(struct hb *buff);
static bool kpb_has_enough_history_data(struct comp_data *kpb,
					struct hb *buff, size_t his_req);
static inline bool kpb_is_sample_width_supported(uint32_t sampling_width);
static void kpb_copy_samples(struct comp_buffer *sink,
			     struct comp_buffer *source, size_t size,
			     size_t sample_width);
static void kpb_drain_samples(void *source, struct comp_buffer *sink,
			      size_t size, size_t sample_width);
static void kpb_reset_history_buffer(struct hb *buff);


/**
 * \brief Create a key phrase buffer component.
 * \param[in] comp - generic ipc component pointer.
 *
 * \return: a pointer to newly created KPB component.
 */
static struct comp_dev *kpb_new(struct sof_ipc_comp *comp)
{
	struct sof_ipc_comp_process *ipc_process =
					(struct sof_ipc_comp_process *)comp;
	size_t bs = ipc_process->size;
	struct comp_dev *dev;
	struct comp_data *kpb;

	trace_kpb("kpb_new()");

	/* Validate input parameters */
	if (IPC_IS_SIZE_INVALID(ipc_process->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_KPB, ipc_process->config);
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	assert(!memcpy_s(&dev->comp, sizeof(struct sof_ipc_comp_process),
			 comp, sizeof(struct sof_ipc_comp_process)));

	kpb = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*kpb));
	if (!kpb) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, kpb);

	assert(!memcpy_s(&kpb->config, sizeof(kpb->config), ipc_process->data,
			 bs));

	if (!kpb_is_sample_width_supported(kpb->config.sampling_width)) {
		trace_kpb_error("kpb_new() error: "
		"requested sampling width not supported");
		return NULL;
	}

	if (kpb->config.no_channels > KPB_MAX_SUPPORTED_CHANNELS) {
		trace_kpb_error("kpb_new() error: "
		"no of channels exceeded the limit");
		return NULL;
	}

	if (kpb->config.sampling_freq != KPB_SAMPLNG_FREQUENCY) {
		trace_kpb_error("kpb_new() error: "
		"requested sampling frequency not supported");
		return NULL;
	}


	/* Init basic component data */
	kpb->history_buffer = NULL;
	kpb->kpb_no_of_clients = 0;

	/* Kpb has been created successfully */
	dev->state = COMP_STATE_READY;
	kpb->state = KPB_STATE_CREATED;

	return dev;
}

/**
 * \brief Allocate history buffer.
 * \param[in] kpb - KPB component data pointer.
 *
 * \return: none.
 */
static size_t kpb_allocate_history_buffer(struct comp_data *kpb)
{
	struct hb *history_buffer;
	struct hb *new_hb = NULL;
	/*! Total allocation size */
	size_t hb_size = kpb->kpb_buffer_size;
	/*! Current allocation size */
	size_t ca_size = hb_size;
	/*! Memory caps priorites for history buffer */
	int hb_mcp[KPB_NO_OF_MEM_POOLS] = {SOF_MEM_CAPS_LP, SOF_MEM_CAPS_HP,
					   SOF_MEM_CAPS_RAM };
	void *new_mem_block = NULL;
	size_t temp_ca_size = 0;
	int i = 0;
	size_t allocated_size = 0;

	trace_kpb("kpb_allocate_history_buffer()");

	/* Initialize history buffer */
	kpb->history_buffer = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
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
		new_mem_block = rballoc(RZONE_RUNTIME, hb_mcp[i], ca_size);

		if (new_mem_block) {
			/* We managed to allocate a block of ca_size.
			 * Now we initialize it.
			 */
			trace_kpb("kpb new memory block: %d", ca_size);
			allocated_size += ca_size;
			history_buffer->start_addr = new_mem_block;
			history_buffer->end_addr = new_mem_block + ca_size;
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
				new_hb = rzalloc(RZONE_RUNTIME,
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

	trace_kpb("kpb_allocate_history_buffer(): allocated %d bytes",
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

	trace_kpb("kpb_free_history_buffer()");

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

	trace_kpb("kpb_free()");

	/* Unregister KPB from async notification */
	notifier_unregister(&kpb->kpb_events);

	/* Reclaim memory occupied by history buffer */
	kpb_free_history_buffer(kpb->history_buffer);
	kpb->history_buffer = NULL;

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
	trace_kpb("kpb_trigger()");

	return comp_set_state(dev, cmd);
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
	size_t allocated_size;

	trace_kpb("kpb_prepare()");

	if (kpb->state == KPB_STATE_RESETTING ||
	    kpb->state == KPB_STATE_RESET_FINISHING) {
		trace_kpb_error("kpb_prepare() error: can not prepare KPB "
				"due to ongoing reset.");
		return -EBUSY;
	}

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* Init private data */
	kpb->kpb_no_of_clients = 0;
	kpb->buffered_data = 0;
	kpb->host_buffer_size = dev->params.buffer.size;
	kpb->host_period_size = dev->params.host_period_bytes;
	kpb->config.sampling_width = dev->params.sample_container_bytes * 8;
	kpb->kpb_buffer_size = KPB_MAX_BUFFER_SIZE(kpb->config.sampling_width);

	if (!kpb->history_buffer) {
		/* Allocate history buffer */
		allocated_size = kpb_allocate_history_buffer(kpb);

		/* Have we allocated what we requested? */
		if (allocated_size < kpb->kpb_buffer_size) {
			trace_kpb_error("Failed to allocate space for "
					"KPB buffer/s");
			kpb_free_history_buffer(kpb->history_buffer);
			kpb->history_buffer = NULL;
			return -EINVAL;
		}
	}
	/* Init history buffer */
	kpb_clear_history_buffer(kpb->history_buffer);

	/* Initialize clients data */
	for (i = 0; i < KPB_MAX_NO_OF_CLIENTS; i++) {
		kpb->clients[i].state = KPB_CLIENT_UNREGISTERED;
		kpb->clients[i].r_ptr = NULL;
	}

	/* Initialize KPB events */
	kpb->kpb_events.id = NOTIFIER_ID_KPB_CLIENT_EVT;
	kpb->kpb_events.cb_data = dev;
	kpb->kpb_events.cb = kpb_event_handler;

	/* Register KPB for async notification */
	notifier_register(&kpb->kpb_events);

	/* Initialize draining task */
	schedule_task_init(&kpb->draining_task, /* task structure */
			   SOF_SCHEDULE_EDF, /* utilize EDF scheduler */
			   0, /* priority doesn't matter for IDLE tasks */
			   kpb_draining_task, /* task function */
			   &kpb->draining_task_data, /* task private data */
			   0, /* core on which we should run */
			   0); /* not used flags */

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
		if (sink->sink->comp.type == SOF_COMP_SELECTOR) {
			/* We found proper real time sink */
			kpb->sel_sink = sink;
		} else if (sink->sink->comp.type == SOF_COMP_HOST) {
			/* We found proper host sink */
			kpb->host_sink = sink;
		}
	}

	kpb->state = KPB_STATE_BUFFERING;

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

static void kpb_cache(struct comp_dev *dev, int cmd)
{
	/* TODO: writeback history buffer */
}

/**
 * \brief Resets KPB component.
 * \param[in,out] dev KPB base component device.
 * \return Error code.
 */
static int kpb_reset(struct comp_dev *dev)
{
	struct comp_data *kpb = comp_get_drvdata(dev);

	trace_kpb("kpb_reset()");

	kpb->buffered_data = 0;
	kpb->is_internal_buffer_full = false;

	/* Change KPB state to RESET. If there is any ongoing job it will
	 * shut itself gracefully first.
	 */
	if (kpb->state == KPB_STATE_BUFFERING ||
	    kpb->state == KPB_STATE_DRAINING) {
		/* KPB is performing some task now,
		 * terminate it gently.
		 */
		kpb->state = KPB_STATE_RESETTING;
		return -EBUSY;
	}

	if (kpb->history_buffer) {
		/* Reset history buffer - zero its data reset pointers
		 * and states.
		 */
		kpb_reset_history_buffer(kpb->history_buffer);
	}

	/* Unregister KPB for async notification */
	notifier_unregister(&kpb->kpb_events);

	/* Finally KPB is ready after reset */
	kpb->state = KPB_STATE_PREPARING;

	return comp_set_state(dev, COMP_TRIGGER_RESET);
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

	tracev_kpb("kpb_copy()");

	/* Get source and sink buffers */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);

	/* Validate source */
	if (!source || !source->r_ptr) {
		trace_kpb_error("kpb_copy(): invalid source pointers.");
		ret = -EINVAL;
		goto out;
	}

	switch (kpb->state) {
	case KPB_STATE_RUN:
		/* In normal RUN state we simply copy to our sink. */
		sink = kpb->sel_sink;

		/* Validate sink */
		if (!sink || !sink->w_ptr) {
			trace_kpb_error("kpb_copy(): invalid selector "
					"sink pointers.");
			ret = -EINVAL;
			goto out;
		}

		copy_bytes = MIN(sink->free, source->avail);
		kpb_copy_samples(sink, source, copy_bytes, sample_width);

		/* Buffer source data internally in history buffer for future
		 * use by clients.
		 */
		if (source->avail <= kpb->kpb_buffer_size) {
			ret = kpb_buffer_data(dev, source, copy_bytes);
			if (ret) {
				trace_kpb_error("kpb_copy(): internal "
						"buffering failed.");
				goto out;
			}
			if (kpb->buffered_data < kpb->kpb_buffer_size)
				kpb->buffered_data += copy_bytes;
			else
				kpb->is_internal_buffer_full = true;
		} else {
			trace_kpb_error("kpb_copy(): too much data to buffer.");
		}

		comp_update_buffer_produce(sink, copy_bytes);
		comp_update_buffer_consume(source, copy_bytes);

		ret = 0;
		break;
	case KPB_STATE_HOST_COPY:
		/* In host copy state we only copy to host buffer. */
		sink = kpb->host_sink;

		/* Validate sink */
		if (!sink || !sink->w_ptr) {
			trace_kpb_error("kpb_copy(): invalid host "
					"sink pointers.");
			ret = -EINVAL;
			goto out;
		}

		copy_bytes = MIN(sink->free, source->avail);
		kpb_copy_samples(sink, source, copy_bytes, sample_width);

		comp_update_buffer_produce(sink, copy_bytes);
		comp_update_buffer_consume(source, copy_bytes);

		ret = 0;
		break;
	case KPB_STATE_DRAINING:
		/* In draining state we only buffer data in internal,
		 * history buffer.
		 */
		if (source->avail <= kpb->kpb_buffer_size) {
			ret = kpb_buffer_data(dev, source, copy_bytes);
			if (ret) {
				trace_kpb_error("kpb_copy(): internal "
						"buffering failed.");
				goto out;
			}

			comp_update_buffer_consume(source, source->avail);
		} else {
			trace_kpb_error("kpb_copy(): too much data to buffer.");
		}

		ret = PPL_STATUS_PATH_STOP;
		break;
	default:
		trace_kpb_error("kpb_copy(): wrong state! Copy forbidden.");
		ret = PPL_STATUS_PATH_STOP;
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
static int kpb_buffer_data(struct comp_dev *dev, struct comp_buffer *source,
			   size_t size)
{
	int ret = 0;
	size_t size_to_copy = size;
	size_t space_avail;
	struct comp_data *kpb = comp_get_drvdata(dev);
	struct hb *buff = kpb->history_buffer;
	void *read_ptr = source->r_ptr;
	size_t timeout = platform_timer_get(platform_timer) +
			 clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1);
	enum kpb_state state_preserved = kpb->state;
	struct dd *draining_data = &kpb->draining_task_data;

	tracev_kpb("kpb_buffer_data()");

	if (kpb->state != KPB_STATE_RUN && kpb->state != KPB_STATE_DRAINING)
		return PPL_STATUS_PATH_STOP;

	kpb->state = KPB_STATE_BUFFERING;

	if (kpb->state == KPB_STATE_DRAINING)
		draining_data->buffered_while_draining += size_to_copy;

	/* Let's store audio stream data in internal history buffer */
	while (size_to_copy) {
		/* Reset was requested, it's time to stop buffering and finish
		 * KPB reset.
		 */
		if (kpb->state == KPB_STATE_RESETTING) {
			kpb->state = KPB_STATE_RESET_FINISHING;
			kpb_reset(dev);
			return PPL_STATUS_PATH_STOP;
		}

		/* Are we stuck in buffering? */
		if (timeout < platform_timer_get(platform_timer)) {
			trace_kpb_error("kpb_buffer_data(): timeout.");
			return -ETIME;
		}

		/* Check how much space there is in current write buffer */
		space_avail = (uint32_t)buff->end_addr - (uint32_t)buff->w_ptr;

		if (size_to_copy > space_avail) {
			/* We have more data to copy than available space
			 * in this buffer, copy what's available and continue
			 * with next buffer.
			 */
			assert(!memcpy_s(buff->w_ptr, space_avail, read_ptr,
					 space_avail));
			/* Update write pointer & requested copy size */
			buff->w_ptr += space_avail;
			size_to_copy = size_to_copy - space_avail;
			/* Update sink read pointer before continuing
			 * with next buffer.
			 */
			read_ptr += space_avail;
		} else {
			/* Requested size is smaller or equal to the space
			 * available in this buffer. In this scenario simply
			 * copy what was requested.
			 */
			assert(!memcpy_s(buff->w_ptr, size_to_copy, read_ptr,
					 size_to_copy));
			/* Update write pointer & requested copy size */
			buff->w_ptr += size_to_copy;
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

	kpb->state = state_preserved;
	return ret;
}

/**
 * \brief Main event dispatcher.
 * \param[in] message - not used.
 * \param[in] cb_data - KPB component internal data.
 * \param[in] event_data - event specific data.
 * \return none.
 */
static void kpb_event_handler(int message, void *cb_data, void *event_data)
{
	(void)message;
	struct comp_dev *dev = (struct comp_dev *)cb_data;
	struct comp_data *kpb = comp_get_drvdata(dev);
	struct kpb_event_data *evd = (struct kpb_event_data *)event_data;
	struct kpb_client *cli = (struct kpb_client *)evd->client_data;

	trace_kpb("kpb_event_handler(): received event with ID: %d ",
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
		trace_kpb_error("kpb_cmd() error: "
				"unsupported command");
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

	trace_kpb("kpb_register_client()");

	if (!cli) {
		trace_kpb_error("kpb_register_client() error: "
				"no client data");
		return -EINVAL;
	}
	/* Do we have a room for a new client? */
	if (kpb->kpb_no_of_clients >= KPB_MAX_NO_OF_CLIENTS ||
	    cli->id >= KPB_MAX_NO_OF_CLIENTS) {
		trace_kpb_error("kpb_register_client() error: "
				"no free room for client = %u ",
				cli->id);
		ret = -EINVAL;
	} else if (kpb->clients[cli->id].state != KPB_CLIENT_UNREGISTERED) {
		trace_kpb_error("kpb_register_client() error: "
				"client = %u already registered",
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
	size_t history_depth = cli->history_depth * kpb->config.no_channels *
			       (kpb->config.sampling_freq / 1000) *
			       (KPB_SAMPLE_CONTAINER_SIZE(sample_width) / 8);
	struct hb *buff = kpb->history_buffer;
	struct hb *first_buff = buff;
	size_t buffered = 0;
	size_t local_buffered = 0;
	enum comp_copy_type copy_type = COMP_COPY_NORMAL;

	trace_kpb("kpb_init_draining()");

	if (kpb->state != KPB_STATE_RUN) {
		trace_kpb_error("kpb_init_draining() error: "
				"wrong KPB state");
	} else if (cli->id > KPB_MAX_NO_OF_CLIENTS) {
		trace_kpb_error("kpb_init_draining() error: "
				"wrong client id");
	/* TODO: check also if client is registered */
	} else if (!is_sink_ready) {
		trace_kpb_error("kpb_init_draining() error: "
				"sink not ready for draining");
	} else if (!kpb_has_enough_history_data(kpb, buff, history_depth)) {
		trace_kpb_error("kpb_init_draining() error: "
				"not enough data in history buffer");
	} else {
		/* Draining accepted, find proper buffer to start reading
		 * At this point we are guaranteed that there is enough data
		 * in the history buffer. All we have to do now is to calculate
		 * read pointer from which we will start draining.
		 */
		do {
			/* Calculate how much data we have stored in
			 * current buffer.
			 */
			local_buffered = 0;
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
				trace_kpb_error("kpb_init_draining() error: "
						"incorrect buffer label");
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
					buff->r_ptr = buff->w_ptr + (buffered -
						      history_depth);
					break;
				}
				buff = buff->prev;
			} else if (history_depth == buffered) {
				buff->r_ptr = buff->start_addr;
				break;
			} else {
				buff->r_ptr = buff->start_addr +
					      (buffered - history_depth);
				break;
			}

		} while (buff != first_buff);

		trace_kpb("kpb_init_draining(), schedule draining task");

		/* Add one-time draining task into the scheduler. */
		kpb->draining_task_data.sink = kpb->host_sink;
		kpb->draining_task_data.history_buffer = buff;
		kpb->draining_task_data.history_depth = history_depth;
		kpb->draining_task_data.state = &kpb->state;
		kpb->draining_task_data.sample_width = sample_width;

		/* Change KPB internal state to DRAINING */
		kpb->state = KPB_STATE_DRAINING;

		/* Set host-sink copy mode to blocking */
		comp_set_attribute(kpb->host_sink->sink, COMP_ATTR_COPY_TYPE,
				   &copy_type);

		/* Schedule draining task */
		schedule_task(&kpb->draining_task, 0, 0,
			      SOF_SCHEDULE_FLAG_IDLE);
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
static uint64_t kpb_draining_task(void *arg)
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
	uint64_t time;
	enum comp_copy_type copy_type = COMP_COPY_NORMAL;

	trace_kpb("kpb_draining_task(), start.");

	time = platform_timer_get(platform_timer);

	while (history_depth > 0) {
		size_to_read = (uint32_t)buff->end_addr - (uint32_t)buff->r_ptr;

		if (size_to_read > sink->free) {
			if (sink->free >= history_depth)
				size_to_copy = history_depth;
			else
				size_to_copy = sink->free;
		} else {
			if (size_to_read >= history_depth) {
				size_to_copy = history_depth;
			} else {
				size_to_copy = size_to_read;
				move_buffer = true;
			}
		}

		kpb_drain_samples(buff->r_ptr, sink, size_to_copy,
				  sample_width);

		buff->r_ptr += (uint32_t)size_to_copy;
		history_depth -= size_to_copy;
		drained += size_to_copy;

		if (move_buffer) {
			buff->r_ptr = buff->start_addr;
			buff = buff->next;
			move_buffer = false;
		}
		if (size_to_copy)
			comp_update_buffer_produce(sink, size_to_copy);
	}

	time =  platform_timer_get(platform_timer) - time;

	/* Draining is done. Now switch KPB to copy real time stream
	 * to client's sink. This state is called "draining on demand"
	 */
	*draining_data->state = KPB_STATE_HOST_COPY;

	/* Reset host-sink copy mode back to unblocking */
	comp_set_attribute(sink->sink, COMP_ATTR_COPY_TYPE, &copy_type);

	trace_kpb("kpb_draining_task(), done. %u drained in %d ms.",
		   drained,
		   time / clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1));

	return 0;
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
static void kpb_drain_samples(void *source, struct comp_buffer *sink,
			       size_t size, size_t sample_width)
{
	int16_t *dest;
	int16_t *src = (int16_t *)source;
	uint32_t i;
	uint32_t j = 0;
	uint32_t channel;
	uint32_t frames = KPB_BYTES_TO_FRAMES(size, sample_width);

	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < KPB_NR_OF_CHANNELS; channel++) {
			dest = buffer_write_frag_s16(sink, j);
			*dest = *src;
			src++;
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

	trace_kpb("kpb_clear_history_buffer()");

	do {
		start_addr = buff->start_addr;
		size = (uint32_t)buff->end_addr - (uint32_t)start_addr;

		bzero(start_addr, size);

		buff = buff->next;
	} while (buff != first_buff);
}

/**
 * \brief Verify if KPB has enough data buffered.
 *
 * \param[in] kpb - KPB component data pointer.
 * \param[in] buff - pointer to current history buffer.
 * \param[in] his_req - requested draining size.
 *
 * \return 1 if there is enough data in history buffer
 *  and 0 otherwise.
 */
static bool kpb_has_enough_history_data(struct comp_data *kpb,
					    struct hb *buff, size_t his_req)
{
	size_t buffered_data = 0;
	struct hb *first_buff = buff;

	/* Quick check if we've already filled internal buffer */
	if (kpb->is_internal_buffer_full)
		return his_req <= kpb->kpb_buffer_size;

	/* Internal buffer isn't full yet. Verify if what already buffered
	 * is sufficient for draining request.
	 */
	while (buffered_data < his_req) {
		if (buff->state == KPB_BUFFER_FREE) {
			if (buff->w_ptr == buff->start_addr &&
			    buff->next->state == KPB_BUFFER_FULL) {
				buffered_data += ((uint32_t)buff->end_addr -
						  (uint32_t)buff->start_addr);
			} else {
				buffered_data += ((uint32_t)buff->w_ptr -
						  (uint32_t)buff->start_addr);
			}

		} else {
			buffered_data += ((uint32_t)buff->end_addr -
					  (uint32_t)buff->start_addr);
		}

		if (buff->next && buff->next != first_buff)
			buff = buff->next;
		else
			break;
	}

	return buffered_data >= his_req;
}

static inline bool kpb_is_sample_width_supported(uint32_t sampling_width)
{
	bool ret;

	switch (sampling_width) {
	case 16:
	/* FALLTHRU */
	case 24:
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
	int16_t *src;
	int16_t *dest;
	uint32_t i;
	uint32_t j = 0;
	uint32_t channel;
	uint32_t frames = KPB_BYTES_TO_FRAMES(size, sample_width);

	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < KPB_NR_OF_CHANNELS; channel++) {
			src = buffer_read_frag_s16(source, j);
			dest = buffer_write_frag_s16(sink, j);
			*dest = *src;
			j++;
		}
	}
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

	trace_kpb("kpb_reset_history_buffer()");

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

struct comp_driver comp_kpb = {
	.type = SOF_COMP_KPB,
	.ops = {
		.new = kpb_new,
		.free = kpb_free,
		.cmd = kpb_cmd,
		.trigger = kpb_trigger,
		.copy = kpb_copy,
		.prepare = kpb_prepare,
		.reset = kpb_reset,
		.cache = kpb_cache,
		.params = NULL,
	},
};

UT_STATIC void sys_comp_kpb_init(void)
{
	comp_register(&comp_kpb);
}

DECLARE_MODULE(sys_comp_kpb_init);
