/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#ifndef __SOF_AUDIO_KPB_H__
#define __SOF_AUDIO_KPB_H__

#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>

struct comp_buffer;

/* KPB tracing */
#define trace_kpb(__e, ...) trace_event(TRACE_CLASS_KPB, __e, ##__VA_ARGS__)
#define trace_kpb_error(__e, ...) trace_error(TRACE_CLASS_KPB, __e, \
					      ##__VA_ARGS__)
#define tracev_kpb(__e, ...) tracev_event(TRACE_CLASS_KPB, __e, ##__VA_ARGS__)
/* KPB internal defines */
#define KPB_MAX_BUFF_TIME 2100 /**< time of buffering in miliseconds */
#define KPB_MAX_SUPPORTED_CHANNELS 2
#define	KPB_SAMPLING_WIDTH 16 /**< number of bits */
#define	KPB_SAMPLNG_FREQUENCY 16000 /* max sampling frequency in Hz */
#define KPB_NR_OF_CHANNELS 2
#define KPB_SAMPLE_CONTAINER_SIZE(sw) ((sw == 16) ? 16 : 32)
#define KPB_MAX_BUFFER_SIZE(sw) ((KPB_SAMPLNG_FREQUENCY / 1000) * \
	(KPB_SAMPLE_CONTAINER_SIZE(sw) / 8) * KPB_MAX_BUFF_TIME * \
	KPB_NR_OF_CHANNELS)
#define KPB_MAX_NO_OF_CLIENTS 2
#define KPB_NO_OF_HISTORY_BUFFERS 2 /**< no of internal buffers */
#define KPB_ALLOCATION_STEP 0x100
#define KPB_NO_OF_MEM_POOLS 3
#define KPB_BYTES_TO_FRAMES(bytes, sample_width) \
	(bytes / ((KPB_SAMPLE_CONTAINER_SIZE(sample_width) / 8) * \
	KPB_NR_OF_CHANNELS))

enum kpb_state {
	KPB_STATE_DISABLED = 0,
	KPB_STATE_RESET_FINISHING,
	KPB_STATE_CREATED,
	KPB_STATE_PREPARING,
	KPB_STATE_RUN,
	KPB_STATE_BUFFERING,
	KPB_STATE_DRAINING,
	KPB_STATE_HOST_COPY,
	KPB_STATE_RESETTING,
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

struct hb {
	enum buffer_state state; /**< state of the buffer */
	void *start_addr; /**< buffer start address */
	void *end_addr; /**< buffer end address */
	void *w_ptr; /**< buffer write pointer */
	void *r_ptr; /**< buffer read pointer */
	struct hb *next; /**< next history buffer */
	struct hb *prev; /**< next history buffer */
};

struct dd {
	struct comp_buffer *sink;
	struct hb *history_buffer;
	size_t history_depth;
	uint8_t is_draining_active;
	size_t sample_width;
	size_t buffered_while_draining;
	size_t drain_interval;
	size_t pb_limit; /**< Period bytes limit */
	struct comp_dev *dev;
	bool sync_mode_on;
};

#ifdef UNIT_TEST
void sys_comp_kpb_init(void);
#endif

#endif /* __SOF_AUDIO_KPB_H__ */
