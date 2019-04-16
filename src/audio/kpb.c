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
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 * A key phrase buffer component.
 */

/**
 * \file audio/kpb.c
 * \brief Key phrase buffer component implementation
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#include <stdint.h>
#include <uapi/ipc/topology.h>
#include <sof/ipc.h>
#include <sof/audio/component.h>
#include <sof/audio/kpb.h>
#include <sof/list.h>
#include <sof/audio/buffer.h>
#include <sof/ut.h>

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
	struct comp_buffer *rt_sink; /**< real time sink (channel selector ) */
	struct comp_buffer *cli_sink; /**< draining sink (client) */
	struct hb *history_buffer;
	bool is_internal_buffer_full;
	size_t buffered_data;
	struct dd draining_task_data;
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

	memcpy(&dev->comp, comp, sizeof(struct sof_ipc_comp_process));

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	memcpy(&cd->config, ipc_process->data, bs);

	if (cd->config.no_channels > KPB_MAX_SUPPORTED_CHANNELS) {
		trace_kpb_error("kpb_new() error: "
		"no of channels exceeded the limit");
		return NULL;
	}

	if (cd->config.history_depth > KPB_MAX_BUFFER_SIZE) {
		trace_kpb_error("kpb_new() error: "
		"history depth exceeded the limit");
		return NULL;
	}

	if (cd->config.sampling_freq != KPB_SAMPLNG_FREQUENCY) {
		trace_kpb_error("kpb_new() error: "
		"requested sampling frequency not supported");
		return NULL;
	}

	if (cd->config.sampling_width != KPB_SAMPLING_WIDTH) {
		trace_kpb_error("kpb_new() error: "
		"requested sampling width not supported");
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
	if (KPB_MAX_BUFFER_SIZE > allocated_size) {
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
	size_t hb_size = KPB_MAX_BUFFER_SIZE;
	/*! Current allocation size */
	size_t ca_size = hb_size;
	/*! Memory caps priorites for history buffer */
	int hb_mcp[KPB_NO_OF_MEM_POOLS] = {SOF_MEM_CAPS_LP, SOF_MEM_CAPS_HP,
					   SOF_MEM_CAPS_RAM };
	void *new_mem_block = NULL;
	size_t temp_ca_size = 0;
	int i = 0;
	size_t allocated_size = 0;

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

	/* Free history buffer/s */
	while (buff) {
		/* first reclaim HB internal memory, then HB itself. */
		if (buff->start_addr)
			rfree(buff->start_addr);

		_buff = buff->next;
		rfree(buff);
		buff = _buff;
	}
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

	cd->kpb_no_of_clients = 0;

	/* init history buffer */
	kpb_clear_history_buffer(cd->history_buffer);

	/* initialize clients data */
	for (i = 0; i < KPB_MAX_NO_OF_CLIENTS; i++) {
		cd->clients[i].state = KPB_CLIENT_UNREGISTERED;
		cd->clients[i].r_ptr = NULL;
	}

	/* initialize KPB events */
	cd->kpb_events.id = NOTIFIER_ID_KPB_CLIENT_EVT;
	cd->kpb_events.cb_data = cd;
	cd->kpb_events.cb = kpb_event_handler;

	/* register KPB for async notification */
	notifier_register(&cd->kpb_events);

	/* initialize draining task */
	/* TODO: this init will be reworked completely once
	 * PR with scheduling for idle tasks is done.
	 */
	schedule_task_init(&cd->draining_task, 0, 0, kpb_draining_task,
			   &cd->draining_task_data, 0, 0);

	/* search for the channel selector sink.
	 * NOTE! We assume here that channel selector component device
	 * is connected to the KPB sinks
	 */
	list_for_item(blist, &dev->bsink_list) {
		sink = container_of(blist, struct comp_buffer, source_list);
		if (!sink->sink) {
			ret = -EINVAL;
			break;
		}
		if (sink->sink->comp.type == SOF_COMP_SELECTOR) {
			/* we found proper sink */
			cd->rt_sink = sink;
			break;
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

	/* Reset history buffer */
	kpb->is_internal_buffer_full = false;
	kpb_clear_history_buffer(kpb->history_buffer);
	/* Reset amount of buffered data */
	kpb->buffered_data = 0;

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

	tracev_kpb("kpb_copy()");

	/* Get source and sink buffers */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = (kpb->state == KPB_STATE_BUFFERING) ? kpb->rt_sink
	       : kpb->cli_sink;

	/* Process source data */
	/* Check if there are valid pointers */
	if (!source || !sink)
		return -EIO;
	if (!source->r_ptr || !sink->w_ptr)
		return -EINVAL;
	/* Check if there is enough free/available space */
	if (sink->free == 0) {
		trace_kpb_error("kpb_copy() error: "
				"sink component buffer"
				" has not enough free bytes for copy");
		comp_overrun(dev, sink, sink->free, 0);
		/* xrun */
		return -EIO;
	}
	if (source->avail == 0) {
		trace_kpb_error("kpb_copy() error: "
				"source component buffer"
				" has not enough data available");
		comp_underrun(dev, source, source->avail, 0);
		/* xrun */
		return -EIO;
	}

	/* Sink and source are both ready and have space */
	copy_bytes = MIN(sink->free, source->avail);
	memcpy(sink->w_ptr, source->r_ptr, copy_bytes);

	/* Buffer source data internally in history buffer for future
	 * use by clients.
	 */
	if (source->avail <= KPB_MAX_BUFFER_SIZE) {
		kpb_buffer_data(kpb, source, copy_bytes);

		if (kpb->buffered_data < KPB_MAX_BUFFER_SIZE)
			kpb->buffered_data += copy_bytes;
		else
			kpb->is_internal_buffer_full = true;
	}

	comp_update_buffer_produce(sink, copy_bytes);
	comp_update_buffer_consume(source, copy_bytes);

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

	tracev_kpb("kpb_buffer_data()");

	/* Let's store audio stream data in internal history buffer */
	while (size_to_copy) {
		/* Check how much space there is in current write buffer */
		space_avail = (uint32_t)buff->end_addr - (uint32_t)buff->w_ptr;

		if (size_to_copy > space_avail) {
			/* We have more data to copy than available space
			 * in this buffer, copy what's available and continue
			 * with next buffer.
			 */
			memcpy(buff->w_ptr, read_ptr, space_avail);
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
			memcpy(buff->w_ptr, read_ptr, size_to_copy);
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
		/* client accepted, let's store his data */
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
	bool is_sink_ready = (kpb->cli_sink->sink->state == COMP_STATE_ACTIVE);
	size_t history_depth = cli->history_depth * kpb->config.no_channels *
			       (kpb->config.sampling_freq / 1000) *
			       (kpb->config.sampling_width / 8);
	struct hb *buff = kpb->history_buffer;
	struct hb *first_buff = buff;
	size_t buffered = 0;
	size_t local_buffered = 0;

	if (cli->id > KPB_MAX_NO_OF_CLIENTS) {
		trace_kpb_error("kpb_init_draining() error: "
				"wrong client id");
		return;
	/* TODO: check also if client is registered */
	} else if (!is_sink_ready) {
		trace_kpb_error("kpb_init_draining() error: "
				"sink not ready for draining");
		return;
	} else if (!kpb_has_enough_history_data(kpb, buff, history_depth)) {
		trace_kpb_error("kpb_init_draining() error: "
				"not enough data in history buffer");

		return;
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

		trace_kpb("kpb_init_draining(), schedule draining r_ptr");

		/* Add one-time draining task into the scheduler. */
		kpb->draining_task_data.sink = kpb->cli_sink;
		kpb->draining_task_data.history_buffer = buff;
		kpb->draining_task_data.history_depth = history_depth;
		kpb->draining_task_data.state = &kpb->state;

		/* Pause selector copy. */
		kpb->rt_sink->sink->state = COMP_STATE_PAUSED;

		/* Set host-sink copy mode to blocking */
		comp_set_attribute(kpb->cli_sink->sink,
				   COMP_ATTR_COPY_BLOCKING, 1);

		/* TODO: schedule draining task */
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
	size_t size_to_read;
	size_t size_to_copy;
	bool move_buffer = false;

	trace_kpb("kpb_draining_task(), start.");

	while (history_depth > 0) {
		size_to_read = (uint32_t)buff->end_addr - (uint32_t)buff->r_ptr;

		if (size_to_read > sink->free) {
			if (sink->free >= history_depth) {
				size_to_copy = history_depth;
			} else {
				size_to_copy = sink->free;
			}
		} else {
			if (size_to_read >= history_depth) {
				size_to_copy = history_depth;
			} else {
				size_to_copy = size_to_read;
				move_buffer = true;
			}
		}

		memcpy(sink->w_ptr, buff->r_ptr, size_to_copy);
		buff->r_ptr += (uint32_t)size_to_copy;
		history_depth -= size_to_copy;

		if (move_buffer) {
			buff->r_ptr = buff->start_addr;
			buff = buff->next;
			move_buffer = false;
		}
		if (size_to_copy)
			comp_update_buffer_produce(sink, size_to_copy);
	}

	/* Draining is done. Now switch KPB to copy real time stream
	 * to client's sink
	 */
	*draining_data->state = KPB_STATE_DRAINING_ON_DEMAND;

	/* Reset host-sink copy mode back to unblocking */
	comp_set_attribute(sink->sink, COMP_ATTR_COPY_BLOCKING, 0);

	trace_kpb("kpb_draining_task(), done.");

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

	do {
		start_addr = buff->start_addr;
		size = start_addr - buff->end_addr;

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
		return his_req <= KPB_MAX_BUFFER_SIZE;

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

DECLARE_COMPONENT(sys_comp_kpb_init);
