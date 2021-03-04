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

/* KPB internal defines */

#ifdef CONFIG_TIGERLAKE
#define KPB_MAX_BUFF_TIME 3000 /**< time of buffering in miliseconds */
#define HOST_WAKEUP_TIME 1000 /* aprox. time of host DMA wakup from suspend [ms] */
#else
/** Due to memory constraints on non-TGL platforms, the buffers are smaller. */
#define KPB_MAX_BUFF_TIME 2100 /**< time of buffering in miliseconds */
#define HOST_WAKEUP_TIME 0 /* aprox. time of host DMA wakup from suspend [ms] */
#endif

#define KPB_MAX_DRAINING_REQ (KPB_MAX_BUFF_TIME - HOST_WAKEUP_TIME)
#define KPB_MAX_SUPPORTED_CHANNELS 2 /**< number of supported channels */
/**< number of samples taken each milisecond */
#define	KPB_SAMPLES_PER_MS (KPB_SAMPLNG_FREQUENCY / 1000)
#define	KPB_SAMPLNG_FREQUENCY 16000 /**< supported sampling frequency in Hz */
#define KPB_NUM_OF_CHANNELS 2
#define KPB_SAMPLE_CONTAINER_SIZE(sw) ((sw == 16) ? 16 : 32)
#define KPB_MAX_BUFFER_SIZE(sw) ((KPB_SAMPLNG_FREQUENCY / 1000) * \
	(KPB_SAMPLE_CONTAINER_SIZE(sw) / 8) * KPB_MAX_BUFF_TIME * \
	KPB_NUM_OF_CHANNELS)
#define KPB_MAX_NO_OF_CLIENTS 2
#define KPB_NO_OF_HISTORY_BUFFERS 2 /**< no of internal buffers */
#define KPB_ALLOCATION_STEP 0x100
#define KPB_NO_OF_MEM_POOLS 3
#define KPB_BYTES_TO_FRAMES(bytes, sample_width) \
	(bytes / ((KPB_SAMPLE_CONTAINER_SIZE(sample_width) / 8) * \
	KPB_NUM_OF_CHANNELS))
/**< Defines how much faster draining is in comparison to pipeline copy. */
#define KPB_DRAIN_NUM_OF_PPL_PERIODS_AT_ONCE 2
/**< Host buffer shall be at least two times bigger than history buffer. */
#define HOST_BUFFER_MIN_SIZE(hb) (hb * 2)

/** All states below as well as relations between them are documented in
 * the sof-dosc in [kpbm-state-diagram]
 * Therefore any addition of new states or modification of existing ones
 * should have a corresponding update in the sof-docs.
 * [kpbm-state-diagram]:
https://thesofproject.github.io/latest/developer_guides/firmware/kd_integration/kd-integration.html#kpbm-state-diagram
"Keyphrase buffer manager state diagram"
 */
enum kpb_state {
	KPB_STATE_DISABLED = 0,
	KPB_STATE_RESET_FINISHING,
	KPB_STATE_CREATED,
	KPB_STATE_PREPARING,
	KPB_STATE_RUN,
	KPB_STATE_BUFFERING,
	KPB_STATE_INIT_DRAINING,
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
	uint32_t drain_req; /**< normalized value of buffered bytes */
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
	enum buffer_state state; /**< state of the buffer */
	void *start_addr; /**< buffer start address */
	void *end_addr; /**< buffer end address */
	void *w_ptr; /**< buffer write pointer */
	void *r_ptr; /**< buffer read pointer */
	struct history_buffer *next; /**< next history buffer */
	struct history_buffer *prev; /**< next history buffer */
};

/* Draining task data */
struct draining_data {
	struct comp_buffer *sink;
	struct history_buffer *hb;
	size_t drain_req;
	uint8_t is_draining_active;
	size_t sample_width;
	size_t buffered_while_draining;
	size_t drain_interval;
	size_t pb_limit; /**< Period bytes limit */
	struct comp_dev *dev;
	bool sync_mode_on;
	enum comp_copy_type copy_type;
};

struct history_data {
	size_t buffer_size; /**< size of internal history buffer */
	size_t buffered; /**< amount of buffered data */
	size_t free; /** spce we can use to write new data */
	struct history_buffer *c_hb; /**< current buffer used for writing */
};

#ifdef UNIT_TEST
void sys_comp_kpb_init(void);
#endif

#endif /* __SOF_AUDIO_KPB_H__ */
