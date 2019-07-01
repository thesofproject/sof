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

#include <stdint.h>
#include <ipc/topology.h>
#include <sof/ipc.h>
#include <sof/audio/component.h>
#include <sof/audio/kpb.h>
#include <sof/list.h>
#include <sof/audio/buffer.h>
#include <sof/ut.h>
#include <sof/clk.h>
#include <sof/agent.h>

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
	size_t period_size;

};

/*! KPB private functions */
static void kpb_event_handler(int message, void *cb_data, void *event_data);
static int kpb_register_client(struct comp_data *kpb, struct kpb_client *cli);
static void kpb_init_draining(struct comp_data *kpb, struct kpb_client *cli);
static uint64_t kpb_draining_task(void *arg);
static void kpb_buffer_data(struct comp_data *kpb, struct comp_buffer *source,
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
	struct comp_data *cd;
	size_t allocated_size;

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

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	assert(!memcpy_s(&cd->config, sizeof(cd->config), ipc_process->data,
			 bs));

	if (!kpb_is_sample_width_supported(cd->config.sampling_width)) {
		trace_kpb_error("kpb_new() error: "
		"requested sampling width not supported");
		return NULL;
	}

	/* Sampling width accepted. Lets calculate and store
	 * its derivatives for quick lookup in runtime.
	 */
	cd->kpb_buffer_size = KPB_MAX_BUFFER_SIZE(cd->config.sampling_width);

	if (cd->config.no_channels > KPB_MAX_SUPPORTED_CHANNELS) {
		trace_kpb_error("kpb_new() error: "
		"no of channels exceeded the limit");
		return NULL;
	}

	if (cd->config.history_depth > cd->kpb_buffer_size) {
		trace_kpb_error("kpb_new() error: "
		"history depth exceeded the limit");
		return NULL;
	}

	if (cd->config.sampling_freq != KPB_SAMPLNG_FREQUENCY) {
		trace_kpb_error("kpb_new() error: "
		"requested sampling frequency not supported");
		return NULL;
	}

	dev->state = COMP_STATE_READY;

	/* Zero number of clients */
	cd->kpb_no_of_clients = 0;

	/* Set initial state as buffering */
	cd->state = KPB_STATE_BUFFERING;

	/* Allocate history buffer */
	allocated_size = kpb_allocate_history_buffer(cd);

	/* Have we allocated what we requested? */
	if (allocated_size < cd->kpb_buffer_size) {
		trace_kpb_error("Failed to allocate space for "
				"KPB buffer/s");
		return NULL;
	}

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
		new_mem_block = rballoc(RZONE_RUNTIME, hb_mcp[i], ca_size + 1);

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

	/* Reclaim memory occupied by history buffer */
	kpb_free_history_buffer(kpb->history_buffer);

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
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;
	int i;
	struct list_item *blist;
	struct comp_buffer *sink;

	trace_kpb("kpb_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* Init private data */
	cd->kpb_no_of_clients = 0;
	cd->buffered_data = 0;
	cd->state = KPB_STATE_BUFFERING;
	cd->host_buffer_size = dev->params.buffer.size;
	cd->period_size = dev->params.host_period_bytes;

	/* Init history buffer */
	kpb_clear_history_buffer(cd->history_buffer);

	/* Initialize clients data */
	for (i = 0; i < KPB_MAX_NO_OF_CLIENTS; i++) {
		cd->clients[i].state = KPB_CLIENT_UNREGISTERED;
		cd->clients[i].r_ptr = NULL;
	}

	/* Initialize KPB events */
	cd->kpb_events.id = NOTIFIER_ID_KPB_CLIENT_EVT;
	cd->kpb_events.cb_data = cd;
	cd->kpb_events.cb = kpb_event_handler;

	/* Register KPB for async notification */
	notifier_register(&cd->kpb_events);

	/* Initialize draining task */
	schedule_task_init(&cd->draining_task, /* task structure */
			   SOF_SCHEDULE_EDF, /* utilize EDF scheduler */
			   0, /* priority doesn't matter for IDLE tasks */
			   kpb_draining_task, /* task function */
			   &cd->draining_task_data, /* task private data */
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
			cd->sel_sink = sink;
		} else if (sink->sink->comp.type == SOF_COMP_HOST) {
			/* We found proper host sink */
			cd->host_sink = sink;
			cd->host_sink->id = 101;
		}
	}

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

	/* Reset state to be buffering */
	kpb->state = KPB_STATE_BUFFERING;
	/* Reset history buffer */
	kpb->is_internal_buffer_full = false;
	kpb_clear_history_buffer(kpb->history_buffer);

	/* Reset amount of buffered data */
	kpb->buffered_data = 0;

	/* Unregister KPB for async notification */
	notifier_unregister(&kpb->kpb_events);

	/* Reset KPB state to initial buffering state */
	kpb->state = KPB_STATE_BUFFERING;

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
	struct comp_buffer *source;
	struct comp_buffer *sink;
	size_t copy_bytes = 0;
	int *debug = (void *)0x9e008000;

	tracev_kpb("kpb_copy()");

	/* Get source and sink buffers */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = (kpb->state == KPB_STATE_BUFFERING) ? kpb->sel_sink
	       : kpb->host_sink;

	/* Stop copying downstream if in draining mode */
	if (kpb->state == KPB_STATE_DRAINING) {
		trace_kpb("RAJWA: upd_hdepth source stats: w_ptr %p, r_ptr %p, avail %d, ho_sink free %d",
			 (uint32_t)source->w_ptr, (uint32_t)source->r_ptr, source->avail, sink->free);
		trace_kpb("RAJWA: sink addres %p", (uint32_t)sink->addr);
		kpb_buffer_data(kpb, source, source->avail);
		comp_update_buffer_consume(source, source->avail);
		return PPL_STATUS_PATH_STOP;
	}

	/* Process source data */
	/* Check if there are valid pointers */
	if (!source || !sink)
		return -EIO;
	if (!source->r_ptr || !sink->w_ptr)
		return -EINVAL;

	/* Sink and source are both ready and have space */
	copy_bytes = MIN(sink->free, source->avail);
	kpb_copy_samples(sink, source, copy_bytes,
			 kpb->config.sampling_width);

	/* Buffer source data internally in history buffer for future
	 * use by clients.
	 */
	if (source->avail <= kpb->kpb_buffer_size) {
		kpb_buffer_data(kpb, source, copy_bytes);
		*(debug) = 0xFEED01;
		*(debug+1) = kpb->buffered_data;
		*(debug+2) = kpb->config.sampling_width;
		*(debug+3) = kpb->kpb_buffer_size;

		if (kpb->buffered_data < kpb->kpb_buffer_size)
			kpb->buffered_data += copy_bytes;
		else {
			kpb->buffered_data += copy_bytes;
			kpb->is_internal_buffer_full = true;
		}
	}

	if (kpb->state == KPB_STATE_HOST_COPY) {
		trace_kpb("RAJWA: host BEFORE cpy: source avail %d host free %d source r_ptr %p",
			   source->avail, sink->free, (uint32_t)source->r_ptr);
		//copy_bytes = source->avail;
	}

	comp_update_buffer_produce(sink, copy_bytes);
	comp_update_buffer_consume(source, copy_bytes);

	if (kpb->state == KPB_STATE_HOST_COPY) {
		trace_kpb("RAJWA: host AFTER cpy: source avail %d host free %d source r_ptr %p",
			   source->avail, sink->free, (uint32_t)source->r_ptr);
		//copy_bytes = source->avail;
	}
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
static void kpb_buffer_data(struct comp_data *kpb, struct comp_buffer *source,
			    size_t size)
{
	size_t size_to_copy = size;
	size_t space_avail;
	struct hb *buff = kpb->history_buffer;
	void *read_ptr = source->r_ptr;
	struct dd *draining_data = &kpb->draining_task_data;

	tracev_kpb("kpb_buffer_data()");

	if (kpb->state == KPB_STATE_DRAINING)
		draining_data->buffered_while_draining += size_to_copy;

	/* Let's store audio stream data in internal history buffer */
	while (size_to_copy) {
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
	struct comp_data *kpb = (struct comp_data *)cb_data;
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
		kpb_init_draining(kpb, cli);
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
static void kpb_init_draining(struct comp_data *kpb, struct kpb_client *cli)
{
	bool is_sink_ready = (kpb->host_sink->sink->state == COMP_STATE_ACTIVE);
	size_t sample_width = kpb->config.sampling_width;
	size_t history_depth = cli->history_depth * kpb->config.no_channels *
			       (kpb->config.sampling_freq / 1000) *
			       (KPB_SAMPLE_CONTAINER_SIZE(sample_width) / 8);
	struct hb *buff = kpb->history_buffer;
	struct hb *first_buff = buff;
	size_t buffered = 0;
	size_t local_buffered = 0;
	size_t period_interval = 0;
	size_t period_size = kpb->period_size;
	size_t host_buffer_size = kpb->host_buffer_size;
	size_t ticks_per_ms = clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1);
	trace_kpb("RAJWA: init draining, history_depth %d", history_depth);
	trace_kpb("kpb_init_draining() host buff size: %d period size %d, ticks_per_ms %d",host_buffer_size, period_size, ticks_per_ms);
	trace_kpb("RAJWA: current w_ptr %p buffered %d ",
			(uint32_t)kpb->history_buffer->w_ptr, kpb->buffered_data);
	if (cli->id > KPB_MAX_NO_OF_CLIENTS) {
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

		/* Calculate time in clock ticks each draining event shall
		 * take place. This time will be used to synchronize us with
		 * an end application interrupts.
		 */
		period_interval = ((host_buffer_size/2)/period_size)*
		                    ticks_per_ms+(ticks_per_ms*5);


		kpb->draining_task_data.period_interval = period_interval;
		kpb->draining_task_data.period_bytes_limit = host_buffer_size/2;

		trace_kpb_error("kpb_init_draining(), period_limit: %d [bytes] and interval %d [uS] %d ticks ",
			   host_buffer_size/2, ((period_interval * 1000) / ticks_per_ms), period_interval);

		/* Add one-time draining task into the scheduler. */
		kpb->draining_task_data.sink = kpb->host_sink;
		kpb->draining_task_data.history_buffer = buff;
		kpb->draining_task_data.history_depth = history_depth;
		kpb->draining_task_data.state = &kpb->state;
		kpb->draining_task_data.sample_width = sample_width;
		kpb->draining_task_data.buffered_while_draining = 0;

		trace_kpb("RAJWA: current w_ptr %p buffered %d ",
			(uint32_t)kpb->history_buffer->w_ptr, kpb->buffered_data);

		/* Pause selector copy. */
		kpb->sel_sink->sink->state = COMP_STATE_PAUSED;

		/* Disable system agent */
		sa_disable();

		/* Change KPB internal state to DRAINING */
		kpb->state = KPB_STATE_DRAINING;

		/* Set host-sink copy mode to blocking */
		comp_set_attribute(kpb->host_sink->sink,
				   COMP_ATTR_COPY_BLOCKING, 0);

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
	uint64_t time_start;
	uint64_t time_end;
	uint64_t period_interval = draining_data->period_interval;
	uint64_t next_copy_time = period_interval * 2 +
				  platform_timer_get(platform_timer);
	uint64_t current_time = 0;
	size_t period_bytes = 0;
	size_t period_bytes_limit = draining_data->period_bytes_limit;
	uint64_t deadline;
	uint32_t attempts = 0;
	size_t *buffered_while_draining = &draining_data->buffered_while_draining;

	trace_kpb("kpb_draining_task(), start buff %p, end buff %p",
		(uint32_t)buff->start_addr, (uint32_t)buff->end_addr);

	time_start = platform_timer_get(platform_timer);
	sink->last_consume = 0;
	sink->last_produce = 0;
	sink->id = 99;
	while (history_depth > 0) {
		if (next_copy_time > platform_timer_get(platform_timer)) {
			period_bytes = 0;
			continue;
		}
		deadline = platform_timer_get(platform_timer) +
		clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) *
		PLATFORM_HOST_DMA_TIMEOUT / 1000;

		while (sink->free && (sink->last_produce != sink->last_consume)) {
			trace_kpb_error("RAJWA: kpb dma copy failed last produce %d last consume %d",
				sink->last_produce, sink->last_consume);
			if (deadline < platform_timer_get(platform_timer)) {
				attempts++;
				if (attempts > 3) {
					trace_kpb_error("We failed to retransmit for 3 times, now skip it.");
					attempts = 0;
					comp_update_buffer_consume(sink, sink->last_produce);
					break;
				}
				trace_kpb_error("RAJWA: attempt to retransmit %d bytes",
					sink->last_produce-sink->last_consume);
				//comp_update_buffer_consume(sink, sink->last_produce);
				comp_update_buffer_produce(sink, 0xFEED);
				deadline = platform_timer_get(platform_timer) +
				clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) *
				PLATFORM_HOST_DMA_TIMEOUT / 1000;
			}
		}


		size_to_read = (uint32_t)buff->end_addr -
			       (uint32_t)buff->r_ptr;

		if (size_to_read > sink->free) {
			if (sink->free >= history_depth)
				size_to_copy = history_depth;
			else
				size_to_copy = sink->free;
		} else {
			if (size_to_read > history_depth) {
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
		/* In theory we should put critical section here */

		drained += size_to_copy;
		period_bytes += size_to_copy;

		if (move_buffer) {
			buff->r_ptr = buff->start_addr;
			buff = buff->next;
			move_buffer = false;
		}
		if (size_to_copy)
			comp_update_buffer_produce(sink, size_to_copy);

		if (period_bytes >= period_bytes_limit) {
			current_time = platform_timer_get(platform_timer);
			next_copy_time = current_time + period_interval;
			/* now process any IPC messages to host */
			ipc_process_msg_queue();

		}

		if (history_depth == 0) {
		trace_kpb("RAJWA: update history_depth by %d",*buffered_while_draining );
		history_depth += *buffered_while_draining;
		*buffered_while_draining = 0;
		deadline = platform_timer_get(platform_timer) +
		clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) *
		PLATFORM_HOST_DMA_TIMEOUT / 1000;

		while (sink->free && (sink->last_produce != sink->last_consume)) {
			trace_kpb_error("RAJWA: kpb dma copy failed last produce %d last consume %d",
				sink->last_produce, sink->last_consume);
			if (deadline < platform_timer_get(platform_timer)) {
				attempts++;
				if (attempts > 3) {
					trace_kpb_error("We failed to retransmit for 3 times, now skip it.");
					attempts = 0;
					comp_update_buffer_consume(sink, sink->last_produce);
					break;
				}
				trace_kpb_error("RAJWA: attempt to retransmit %d bytes",
					sink->last_produce-sink->last_consume);
				//comp_update_buffer_consume(sink, sink->last_produce);
				comp_update_buffer_produce(sink, 0xFEED);
				deadline = platform_timer_get(platform_timer) +
				clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) *
				PLATFORM_HOST_DMA_TIMEOUT / 1000;
			}
		}
		}
	}

	time_end = platform_timer_get(platform_timer);

	/* Draining is done. Now switch KPB to copy real time stream
	 * to client's sink. This state is called "draining on demand"
	 */
	*draining_data->state = KPB_STATE_HOST_COPY;

	/* Reset host-sink copy mode back to unblocking */
	comp_set_attribute(sink->sink, COMP_ATTR_COPY_BLOCKING, 0);

	trace_kpb("RAJWA: kpb_draining_task(), done. %u drained in %d ms, r_ptr %p",
		   drained,
		   (time_end - time_start)
		   / clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1),
		   (uint32_t)buff->r_ptr);

	/* Enable system agent back */
	sa_enable();
	return 0;
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
	case 32:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
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
	void *dst;
	void *src = source;
	size_t i;
	size_t j = 0;
	size_t channel;
	size_t frames = KPB_BYTES_TO_FRAMES(size, sample_width);

	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < KPB_NR_OF_CHANNELS; channel++) {
			if (sample_width == 16) {
				dst = buffer_write_frag_s16(sink, j);
				*((int16_t *)dst) = *((int16_t *)src);
				src = ((int16_t *)src) + 1;
			} else if (sample_width == 24) {
				dst = buffer_write_frag_s32(sink, j);
				*((int32_t *)dst) = *(int32_t *)src >> 8 &
						    0x00FFFFFF;
				src = ((int32_t *)src) + 1;
			} else if (sample_width == 32) {
				dst = buffer_write_frag_s32(sink, j);
				*((int32_t *)dst) = *((int32_t *)src);
				src = ((int32_t *)src) + 1;
			} else {
				trace_kpb_error("KPB: An attempt to copy"
				                "not supported format!");
				return;
			}
			j++;
		}
	}
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
	void *dst;
	void *src;
	size_t i;
	size_t j = 0;
	size_t channel;
	size_t frames = KPB_BYTES_TO_FRAMES(size, sample_width);

	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < KPB_NR_OF_CHANNELS; channel++) {
			if (sample_width == 16) {
				dst = buffer_write_frag_s16(sink, j);
				src = buffer_read_frag_s16(source, j);
				*((int16_t *)dst) = *((int16_t *)src);
				//src = ((int16_t *)src) + 1;
			} else if (sample_width == 24) {
				dst = buffer_write_frag_s32(sink, j);
				src = buffer_read_frag_s32(source, j);
				*((int32_t *)dst) = *(int32_t *)src >> 8 &
						    0x00FFFFFF;
				//src = ((int32_t *)src) + 1;
			} else if (sample_width == 32) {
				dst = buffer_write_frag_s32(sink, j);
				src = buffer_read_frag_s32(source, j);
				*((int32_t *)dst) = *((int32_t *)src);
				//src = ((int32_t *)src) + 1;
			} else {
				trace_kpb_error("KPB: An attempt to copy"
				                "not supported format!");
				return;
			}
			j++;
		}
	}
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
