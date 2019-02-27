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
 */

#ifndef __INCLUDE_AUDIO_KPB_H__
#define __INCLUDE_AUDIO_KPB_H__

#include <platform/platcfg.h>
#include <sof/notifier.h>
#include <sof/trace.h>

/* KPB tracing */
#define trace_kpb(__e, ...) trace_event(TRACE_CLASS_KPB, __e, ##__VA_ARGS__)
#define trace_kpb_error(__e, ...) (trace_error(TRACE_CLASS_KPB, __e, \
					      ##__VA_ARGS__))
#define tracev_kpb(__e, ...) tracev_event(TRACE_CLASS_KPB, __e, ##__VA_ARGS__)
/* KPB internal defines */
#define KPB_MAX_BUFF_TIME 2100 /**< time of buffering in miliseconds */
#define KPB_MAX_SUPPORTED_CHANNELS 2
#define	KPB_SAMPLING_WIDTH 16 /**< number of bits */
#define	KPB_SAMPLNG_FREQUENCY 16000 /* max sampling frequency in Hz */
#define KPB_NR_OF_CHANNELS 2
#define KPB_SAMPLE_CONTAINER_SIZE ((KPB_SAMPLING_WIDTH == 32) ? 64 : 32)
#define KPB_MAX_BUFFER_SIZE ((KPB_SAMPLNG_FREQUENCY / 1000) * \
	(KPB_SAMPLE_CONTAINER_SIZE / 8) * KPB_MAX_BUFF_TIME * \
	KPB_NR_OF_CHANNELS)
#define KPB_MAX_NO_OF_CLIENTS 2
#define KPB_NO_OF_HISTORY_BUFFERS 2 /**< no of internal buffers */

enum kpb_sink_state {
	KPB_SINK_BUSY = 0,
	KPB_SINK_READY,
};

struct kpb_sink {
	uint32_t *data_ptr;
	uint32_t data_size;
	enum kpb_sink_state state;
};

enum kpb_event {
	KPB_EVENT_REGISTER_CLIENT = 0,
	KPB_EVENT_UPDATE_PARAMS,
	KPB_EVENT_BEGIN_DRAINING,
	KPB_EVENT_STOP_DRAINING,
	KPB_EVENT_UNREGISTER_CLIENT,
};

struct kpb_event_data {
	enum kpb_event event_id;
	struct kpb_client *client_data;
};

enum kpb_client_state {
	KPB_CLIENT_UNREGISTERED = 0,
	KPB_CLIENT_BUFFERING,
	KPB_CLIENT_DRAINNING,
	KPB_CLIENT_DRAINNING_OD, /**< draining on demand */
};

struct kpb_client {
	uint8_t id; /**< id associated with output sink */
	uint32_t history_depth; /**< normalized value of buffered bytes */
	uint32_t history_begin; /**< place where key phrase begins */
	uint32_t history_end; /**< place where key phrase ends */
	enum kpb_client_state state; /**< current state of a client */
	void *r_ptr; /**< current read position */
	struct comp_buffer *sink; /**< client's sink */
};

enum buffer_state {
	KPB_BUFFER_FREE = 0,
	KPB_BUFFER_FULL,
	KPB_BUFFER_OFF,
};

enum kpb_id {
	KPB_LP = 0,
	KPB_HP,
};

struct history_buffer {
	enum kpb_id id;
	enum buffer_state state;
	void *w_ptr; /**< buffer write pointer */
	void *r_ptr; /**< buffer read pointer */
	void *sta_addr;
	void *end_addr; /**< buffer end address */

};

/*! Key phrase buffer component */
struct comp_data {
	/* runtime data */
	uint8_t no_of_clients; /**< number of registered clients */
	struct kpb_client clients[KPB_MAX_NO_OF_CLIENTS];
	struct history_buffer his_buf_lp;
	struct history_buffer his_buf_hp;
	struct notifier kpb_events; /**< KPB events object */
	uint32_t source_period_bytes; /**< source number of period bytes */
	uint32_t sink_period_bytes; /**< sink number of period bytes */

};

#endif
