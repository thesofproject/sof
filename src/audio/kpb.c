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

static void kpb_event_handler(int message, void *cb_data, void *event_data);
static int kpb_register_client(struct kpb_comp_data *kpb,
			       struct kpb_client *cli);
static void kpb_init_draining(struct kpb_comp_data *kpb,
			      struct kpb_client *cli);
static uint64_t kpb_draining_task(void *arg);
static void kpb_buffer_data(struct kpb_comp_data *kpb,
			    struct comp_buffer *source);

/**
 * \brief Create a key phrase buffer component.
 * \param[in] comp - generic ipc component pointer.
 *
 * \return: a pointer to newly created KPB component.
 */
static struct comp_dev *kpb_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_kpb *kpb;
	struct sof_ipc_comp_kpb *ipc_kpb = (struct sof_ipc_comp_kpb *)comp;
	struct kpb_comp_data *cd;

	trace_kpb("kpb_new()");

	/* Validate input parameters */
	if (IPC_IS_SIZE_INVALID(ipc_kpb->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_KPB, ipc_kpb->config);
		return NULL;
	}

	if (ipc_kpb->no_channels > KPB_MAX_SUPPORTED_CHANNELS) {
		trace_kpb_error("kpb_new() error: "
		"no of channels exceeded the limit");
		return NULL;
	}

	if (ipc_kpb->history_depth > KPB_MAX_BUFFER_SIZE) {
		trace_kpb_error("kpb_new() error: "
		"history depth exceeded the limit");
		return NULL;
	}

	if (ipc_kpb->sampling_freq != KPB_SAMPLNG_FREQUENCY) {
		trace_kpb_error("kpb_new() error: "
		"requested sampling frequency not supported");
		return NULL;
	}

	if (ipc_kpb->sampling_width != KPB_SAMPLING_WIDTH) {
		trace_kpb_error("kpb_new() error: "
		"requested sampling width not supported");
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_kpb));
	if (!dev)
		return NULL;

	kpb = (struct sof_ipc_comp_kpb *)&dev->comp;
	memcpy(kpb, ipc_kpb, sizeof(struct sof_ipc_comp_kpb));

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}
	comp_set_drvdata(dev, cd);
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
	struct kpb_comp_data *kpb = comp_get_drvdata(dev);

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
	struct kpb_comp_data *cd = comp_get_drvdata(dev);
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
	int hb_mcp[KPB_NO_OF_MEM_POOLS] = {SOF_MEM_CAPS_LP, SOF_MEM_CAPS_HP,
				       SOF_MEM_CAPS_RAM};
	void *ptr = NULL;

	trace_kpb("kpb_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

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
						 COMP_SIZE(struct hb));
				history_buffer->next = new_hb;
				new_hb->state = KPB_BUFFER_OFF;
				new_hb->next = history_buffer;
				history_buffer = new_hb;
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
			ca_size = ca_size - KPB_ALLOCATION_STEP;
			if (ca_size <= 0) {
				ca_size = hb_size;
				i++;
			}
			continue;
		}
	}

	/* have we succeed in allocation of at least one buffer? */
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

	/* initialie draining task */
	schedule_task_init(&cd->draining_task, SOF_SCHEDULE_EDF, 0,
			   kpb_draining_task, cd, 0, 0);

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
	struct kpb_comp_data *kpb = comp_get_drvdata(dev);
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
		} else if (sink->free < kpb->sink_period_bytes) {
			trace_kpb_error("kpb_copy() error: "
				   "sink component buffer"
				   " has not enough free bytes for copy");
			comp_overrun(dev, sink, kpb->sink_period_bytes, 0);
			/* xrun */
			return -EIO;
		} else if (source->avail < kpb->source_period_bytes) {
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
			       kpb->sink_period_bytes);
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
		comp_update_buffer_produce(sink, kpb->sink_period_bytes);
		comp_update_buffer_consume(source, kpb->sink_period_bytes);
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
static void kpb_buffer_data(struct kpb_comp_data *kpb,
			    struct comp_buffer *source)
{
	int size_to_copy = kpb->source_period_bytes;
	int space_avail;
	struct hb *buff = &kpb->history_buffer;
	struct hb *first_buff = buff;

	trace_kpb("kpb_buffer_data()");

	/* find free buffer */
	while (buff->next && buff->next != first_buff) {
		if (buff->state == KPB_BUFFER_FREE)
			break;
		else
			buff = buff->next;
	}

	while (size_to_copy) {
		space_avail = (int)buff->end_addr - (int)buff->w_ptr;

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
	struct kpb_comp_data *kpb = (struct kpb_comp_data *)cb_data;
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
static int kpb_register_client(struct kpb_comp_data *kpb,
			       struct kpb_client *cli)
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
static void kpb_init_draining(struct kpb_comp_data *kpb, struct kpb_client *cli)
{
	uint8_t is_sink_ready = (kpb->clients[cli->id].sink->sink->state
				 == COMP_STATE_ACTIVE) ? 1 : 0;

	if (kpb->clients[cli->id].state == KPB_CLIENT_UNREGISTERED) {
		/* TODO: possibly move this check to draining task
		 * the doubt is if HOST managed to change the sink state
		 * at notofication time
		 */
		trace_kpb_error("kpb_init_draining() error: "
				"requested draining for unregistered client");
	} else if (!is_sink_ready) {
		trace_kpb_error("kpb_init_draining() error: "
				"sink not ready for draining");
	} else {
		/* add one-time draining task into the scheduler */
		schedule_task(&kpb->draining_task, 0, 0, 0);
	}
}

static uint64_t kpb_draining_task(void *arg)
{
	/* TODO: while loop drainning history buffer accoriding to
	 * clients request
	 */
	return 0;
}

static int kpb_params(struct comp_dev *dev)
{
	/*TODO*/

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

UT_STATIC void sys_comp_kpb_init(void)
{
	comp_register(&comp_kpb);
}

DECLARE_COMPONENT(sys_comp_kpb_init);
