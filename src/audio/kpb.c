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
#include <sof/audio/buffer.h>
#include <sof/audio/kpb.h>
#include <sof/list.h>

/*! Key phrase buffer component */
struct comp_data {
	/* runtime data */
	uint8_t no_of_clients; /**< number of registered clients */
	struct kpb_client clients[KPB_MAX_NO_OF_CLIENTS];
	struct hb history_buffer;
	struct notifier kpb_events; /**< KPB events object */
	struct task draining_task;
	struct dd draining_task_data;
	uint32_t source_period_bytes; /**< source number of period bytes */
	uint32_t sink_period_bytes; /**< sink number of period bytes */
	struct sof_kpb_config config;   /**< component configuration data */
	struct comp_buffer *rt_sink; /**< real time sink (channel selector ) */
	struct comp_buffer *cli_sink; /**< draining sink (client) */

};

static void kpb_event_handler(int message, void *cb_data, void *event_data);
static int kpb_register_client(struct comp_data *kpb, struct kpb_client *cli);
static void kpb_init_draining(struct comp_data *kpb, struct kpb_client *cli);
static void draining_task(void *arg);
static void kpb_buffer_data(struct comp_data *kpb, struct comp_buffer *source);
static uint8_t kpb_have_enough_history_data(struct hb *buff, size_t his_req);

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

	return dev;
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

	/* TODO: reclaim internal buffer memory */
	/* reclaim device & component data memory */
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
	int i = 0;
	struct list_item *blist;
	struct comp_buffer *sink;
	struct hb *history_buffer = &cd->history_buffer;
	struct hb *new_hb = NULL;
	/*! total allocation size */
	size_t hb_size = KPB_MAX_BUFFER_SIZE;
	/*! current allocation size */
	size_t ca_size = hb_size;
	/*! memory caps priorites for history buffer */
	int hb_mcp[KPB_NO_OF_MEM_POOLS] = {SOF_MEM_CAPS_LP, SOF_MEM_CAPS_HP, SOF_MEM_CAPS_RAM };
	void *ptr = NULL;
	size_t _temp = 0;

	trace_kpb("kpb_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret)
		return ret;

	/* Initialize history buffer */
	cd->history_buffer.next = &cd->history_buffer;
	cd->history_buffer.prev = &cd->history_buffer;




	cd->no_of_clients = 0;
	/* allocate history buffer/s */
	while (hb_size > 0 && i < ARRAY_SIZE(hb_mcp)) {

		ptr = rballoc(RZONE_RUNTIME, hb_mcp[i], ca_size);

		if (ptr) {
			history_buffer->start_addr = ptr;
			history_buffer->end_addr = ptr + ca_size;
			history_buffer->w_ptr = ptr;
			history_buffer->r_ptr = ptr;
			history_buffer->state = KPB_BUFFER_FREE;
			hb_size = hb_size - ca_size;
			history_buffer->next = &cd->history_buffer;
			/* Do we need another buffer? */
			if (hb_size > 0) {

				new_hb = rzalloc(RZONE_RUNTIME,
						 SOF_MEM_CAPS_RAM,
						 sizeof(struct hb));

				history_buffer->next = new_hb;
				new_hb->state = KPB_BUFFER_OFF;
				new_hb->prev = history_buffer;
				history_buffer = new_hb;
				cd->history_buffer.prev = new_hb;
				ca_size = hb_size;
				i++;
			}

		} else {
			/* we've failed to allocate ca_size of that hb_mcp
			 * let's try again with some smaller size.
			 * NOTE! If we decrement by some small value,
			 * the allocation will take significant time.
			 * However, bigger values like HEAP_HP_BUFFER_BLOCK_SIZE
			 * will result in lower accuracy of allocation.
			 */
			_temp = (ca_size - KPB_ALLOCATION_STEP);
			ca_size = (ca_size < _temp) ? 0 : _temp;
			if (ca_size == 0) {
				ca_size = hb_size;
				i++;
			}
			continue;
		}
	}

	/* have we succeed in allocation of at least one buffer? */
	/*TODO: check if we have allocated as much as we wanted */
	if (!cd->history_buffer.start_addr) {
		trace_kpb_error("Failed to allocate space for "
				"KPB bufefr/s");
		return -ENOMEM;
	}

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
	schedule_task_init(&cd->draining_task, draining_task,
			   &cd->draining_task_data);
	schedule_task_config(&cd->draining_task, TASK_PRI_MED, 0);

	/* search for KPB related sinks.
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
			/* we found proper real time sink */
			cd->rt_sink = sink;
		} else if (sink->sink->comp.type == SOF_COMP_HOST) {
			cd->cli_sink = sink;
		} else {
			continue;
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

static int kpb_reset(struct comp_dev *dev)
{
	/* TODO: what data of KPB should we reset here? */
	return 0;
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
	int update_buffers = 0;
	struct comp_data *kpb = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;

	tracev_kpb("kpb_copy()");

	/* get source and sink buffers */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = kpb->rt_sink;

	/* process source data */
	/* check if there are valid pointers */
	if (source && sink) {
		if (!source->r_ptr || !sink->w_ptr) {
			return -EINVAL;
		} else if (sink->free < kpb->sink_period_bytes && 0) {
			/* TODO: to be removed */
			trace_kpb_error("kpb_copy() error: "
				   "sink component buffer"
				   " has not enough free bytes for copy");
			comp_overrun(dev, sink, kpb->sink_period_bytes, 0);
			/* xrun */
			return -EIO;
		} else if (source->avail < kpb->source_period_bytes && 0) {
			/* TODO: to be removed */
			/* xrun */
			trace_kpb_error("kpb_copy() error: "
					   "source component buffer"
					   " has not enough data available");
			comp_underrun(dev, source, kpb->source_period_bytes,
				      0);
			return -EIO;
		} else {
			/* sink and source are both ready and have space */
			/* TODO: copy sink or source period data here? */
			memcpy(sink->w_ptr, source->r_ptr,
			       MIN(sink->free, source->avail));
			/* signal update source & sink data */
			update_buffers = 1;
		}
		/* buffer source data internally for future use by clients */
		if (source->avail <= KPB_MAX_BUFFER_SIZE) {
			/* TODO: should we copy what is available or just
			 * a small portion of it?
			 */
			kpb_buffer_data(kpb, source);
		}
	} else {
		ret = -EIO;
	}

	if (update_buffers) {
		comp_update_buffer_produce(sink, MIN(sink->free, source->avail));
		comp_update_buffer_consume(source, MIN(sink->free, source->avail));
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
 * \return none
 */
static void kpb_buffer_data(struct comp_data *kpb, struct comp_buffer *source)
{
	int size_to_copy = kpb->source_period_bytes;
	int space_avail;
	struct hb *buff = &kpb->history_buffer;
	struct hb *first_buff = buff;
	tracev_kpb("kpb_buffer_data()");

	/* find free buffer */
	while (buff->next && buff->next != first_buff) {
		if (buff->state == KPB_BUFFER_FREE)
			break;
		else
			buff = buff->next;
	}

	while (size_to_copy) {
		space_avail = (uint32_t)buff->end_addr - (uint32_t)buff->w_ptr;
		if (size_to_copy > space_avail) {
			memcpy(buff->w_ptr, source->r_ptr, space_avail);
			size_to_copy = size_to_copy - space_avail;
			buff->w_ptr = buff->start_addr;

			if (buff->next && buff->next != buff) {
				buff->state = KPB_BUFFER_FULL;
				buff = buff->next;
				buff->state = KPB_BUFFER_FREE;
			} else {
				buff->state = KPB_BUFFER_FREE;
			}
		} else {
			memcpy(buff->w_ptr, source->r_ptr, size_to_copy);
			buff->w_ptr += size_to_copy;

			if (buff->w_ptr == buff->end_addr) {
				buff->w_ptr = buff->start_addr;

				if (buff->next && buff->next != buff) {
					buff->state = KPB_BUFFER_FULL;
					buff = buff->next;
					buff->state = KPB_BUFFER_FREE;
				} else {
					buff->state = KPB_BUFFER_FREE;
				}
			}
			size_to_copy = 0;
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

	tracev_kpb("kpb_event_handler()");

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
	if (kpb->no_of_clients >= KPB_MAX_NO_OF_CLIENTS ||
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
		kpb->no_of_clients++;
		ret = 0;
	}

	return ret;
}

/**
 * \brief Drain internal buffer into client's sink buffer.
 *
 * \param[in] kpb - kpb component data.
 * \param[in] cli - client's data.
 *
 * \return integer representing either:
 *	0 - success
 *	-EINVAL - failure.
 */
static void kpb_init_draining(struct comp_data *kpb, struct kpb_client *cli)
{
	uint8_t is_sink_ready = (kpb->cli_sink->sink->state
                                 == COMP_STATE_ACTIVE) ? 1 : 0;
	size_t history_depth = cli->history_depth;
	struct hb *buff = &kpb->history_buffer;
	struct hb *first_buff = buff;
	size_t buffered = 0;
 	size_t local_buff_buffered_size = 0;

	if (cli->id > KPB_MAX_NO_OF_CLIENTS) {
		trace_kpb_error("kpb_init_draining() error: "
				"wrong client id");

		return;
	} else if (kpb->clients[cli->id].state == KPB_CLIENT_UNREGISTERED) {
		/* TODO: possibly move this check to draining task
		 * the doubt is if HOST managed to change the sink state
		 * at notofication time
		 */
		trace_kpb_error("kpb_init_draining() error: "
				"requested draining for unregistered client");

		return;
	} else if (!is_sink_ready) {
		trace_kpb_error("kpb_init_draining() error: "
				"sink not ready for draining");
		return;
	} else if (!kpb_have_enough_history_data(buff, history_depth)) {
		trace_kpb_error("kpb_init_draining() error: "
				"not enough data in history buffer");

		return;
	} else {

		/* draining accepted, find proper buffer to start reading */
		while (buff->next && buff->next != first_buff) {
			if (buff->state == KPB_BUFFER_FREE)
				break;
			else
				buff = buff->next;
		}

		/* calculate read pointer*/
		first_buff = buff;
		do {
			local_buff_buffered_size = 0;

			if (buff->state == KPB_BUFFER_FREE) {
				local_buff_buffered_size = (uint32_t)buff->w_ptr - (uint32_t)buff->start_addr;
				buffered += local_buff_buffered_size;
			} else if (buff->state == KPB_BUFFER_FULL) {
				local_buff_buffered_size = (uint32_t)buff->end_addr - (uint32_t)buff->start_addr;
				buffered += local_buff_buffered_size;
			} else {
				trace_kpb_error("kpb_init_draining() error: "
				"incorrect buffer label");
			}

			if (history_depth > buffered) {
				buff = buff->prev;
			} else if (history_depth == buffered) {
				buff->r_ptr = buff->start_addr;
				break;
			} else {
				buff->r_ptr = buff->start_addr + (buffered - history_depth);
			}
		} while (buff != first_buff);

		/* add one-time draining task into the scheduler */
		kpb->draining_task_data.sink = kpb->cli_sink;
		kpb->draining_task_data.history_buffer = buff;
		kpb->draining_task_data.history_depth = history_depth;
		schedule_task_idle(&kpb->draining_task, 100);
	}
}

static void draining_task(void *arg)
{

	struct dd *draining_data = (struct dd *)arg;
	struct comp_buffer *sink = draining_data->sink;


	struct hb *buff = draining_data->history_buffer;
	size_t size_to_read;
	size_t size_to_copy;

	while (draining_data->history_depth > 0 && sink->avail > 0) {
		size_to_read = (uint32_t)buff->end_addr - (uint32_t)buff->r_ptr;
		if (size_to_read > sink->avail) {
			if (sink->avail >= draining_data->history_depth)
				size_to_copy = draining_data->history_depth;
			else
				size_to_copy = sink->avail;

			memcpy(sink->w_ptr, buff->r_ptr, size_to_copy);
			buff->r_ptr += (uint32_t)size_to_copy;
			draining_data->history_depth -= size_to_copy;

			comp_update_buffer_produce(sink, size_to_copy);
		} else {
			if (size_to_read >= draining_data->history_depth)
				size_to_copy = draining_data->history_depth;
			else
				size_to_copy = size_to_read;

			memcpy(sink->w_ptr, buff->r_ptr, size_to_copy);
			buff->r_ptr = buff->start_addr;
			draining_data->history_depth -= size_to_copy;
			draining_data->history_buffer = buff->next;

			comp_update_buffer_produce(sink, size_to_copy);
		}
	}

}

static uint8_t kpb_have_enough_history_data(struct hb *buff, size_t his_req)
{
	uint8_t ret = 0;
	size_t buffered_data = 0;
	struct hb *first_buff = buff;

	while (buffered_data < his_req) {
		if (buff->state == KPB_BUFFER_FREE) {
			if (buff->w_ptr == buff->start_addr &&
			    buff->next->state == KPB_BUFFER_FULL)
			{
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
	ret = (buffered_data >= his_req) ? 1 : 0;
	return ret;
}

static int kpb_params(struct comp_dev *dev)
{
	trace_kpb("kpb_params()");

	return 0;
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
		.params = kpb_params,
	},
};
void sys_comp_kpb_init(void);

void sys_comp_kpb_init(void)
{
	comp_register(&comp_kpb);
}

DECLARE_COMPONENT(sys_comp_kpb_init);
