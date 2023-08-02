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

#if defined(__XCC__)

#include <xtensa/config/core-isa.h>
#if XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4
#define KPB_HIFI3
#endif

#endif
struct comp_buffer;

/* KPB internal defines */

#if CONFIG_TIGERLAKE
#define KPB_MAX_BUFF_TIME 3000 /**< time of buffering in miliseconds */
#define HOST_WAKEUP_TIME 1000 /* aprox. time of host DMA wakup from suspend [ms] */
#else
/** Due to memory constraints on non-TGL platforms, the buffers are smaller. */
#define KPB_MAX_BUFF_TIME 2100 /**< time of buffering in miliseconds */
#define HOST_WAKEUP_TIME 0 /* aprox. time of host DMA wakup from suspend [ms] */
#endif

#define KPB_MAX_DRAINING_REQ (KPB_MAX_BUFF_TIME - HOST_WAKEUP_TIME)
#define KPB_MAX_SUPPORTED_CHANNELS 6 /**< number of supported channels */
/**< number of samples taken each milisecond */
#define	KPB_SAMPLES_PER_MS (KPB_SAMPLNG_FREQUENCY / 1000)
#define	KPB_SAMPLNG_FREQUENCY 16000 /**< supported sampling frequency in Hz */
#define KPB_SAMPLE_CONTAINER_SIZE(sw) ((sw == 16) ? 16 : 32)
#define KPB_MAX_BUFFER_SIZE(sw, channels_number) ((KPB_SAMPLNG_FREQUENCY / 1000) * \
	(KPB_SAMPLE_CONTAINER_SIZE(sw) / 8) * KPB_MAX_BUFF_TIME * \
	 (channels_number))
#define KPB_MAX_NO_OF_CLIENTS 2
#define KPB_MAX_SINK_CNT (1 + KPB_MAX_NO_OF_CLIENTS)
#define KPB_NO_OF_HISTORY_BUFFERS 2 /**< no of internal buffers */
#define KPB_ALLOCATION_STEP 0x100
#define KPB_NO_OF_MEM_POOLS 3
#define KPB_BYTES_TO_FRAMES(bytes, sample_width, channels_number) \
	((bytes) / ((KPB_SAMPLE_CONTAINER_SIZE(sample_width) / 8) * \
	 (channels_number)))
/**< Defines how much faster draining is in comparison to pipeline copy. */
#define KPB_DRAIN_NUM_OF_PPL_PERIODS_AT_ONCE 2
/**< Host buffer shall be at least two times bigger than history buffer. */
#define HOST_BUFFER_MIN_SIZE(hb, channels_number) ((hb) * (channels_number))

/**< Convert with right shift a bytes count to samples count */
#define KPB_BYTES_TO_S16_SAMPLES(s)	((s) >> 1)
#define KPB_BYTES_TO_S32_SAMPLES(s)	((s) >> 2)

/* Macro that returns mask with selected bits set */
#define KPB_COUNT_TO_BITMASK(cnt) (((0x1 << (cnt)) - 1))
#define KPB_IS_BIT_SET(value, idx) ((value) & (1 << (idx)))
#define KPB_REFERENCE_SUPPORT_CHANNELS 2
/* Maximum number of channels in the micsel mask is 4,
 * i.e. number of max supported channels - reference channels)
 */
#define KPB_MAX_MICSEL_CHANNELS 4
/* Used in FMT */
#define FAST_MODE_TASK_MAX_MODULES_COUNT 16
#define REALTIME_PIN_ID 0

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

/* moved to ipc4/kpb.h */
/* enum ipc4_kpb_module_config_params */

/* Stores KPB mic selector config */
struct kpb_micselector_config {
	/* channel bit set to 1 implies channel selection */
	uint32_t mask;
};

struct kpb_task_params {
	/* If largeconfigset is set to KP_POS_IN_BUFFER then number of modules must
	 * correspond to number of modules between kpb and copier attached to hostdma.
	 * Once draining path is configured, cannot be reinitialized/changed.
	 */
	uint32_t number_of_modules;
	struct {
		uint16_t module_id;
		uint16_t instance_id;
	} dev_ids[1];
};

/* fmt namespace: */
#define FAST_MODE_TASK_MAX_LIST_COUNT 5

struct fast_mode_task {
	/*! Array of pointers to all module lists to be processed. */
	struct device_list *device_list[FAST_MODE_TASK_MAX_LIST_COUNT];
};

/* The +1 is here because we also push the kbp device
 * handle in addition to the max number of modules
 */
#define DEVICE_LIST_SIZE (FAST_MODE_TASK_MAX_MODULES_COUNT + 1)

/* Devicelist type
 * In Reference FW KPB used Bi-dir lists to store modules for FMT. It is possible that lists are
 * not necessary, but in case it might be wrong, here we use an array
 * with a list interface to switch it to a list easily.
 */
struct device_list {
	struct comp_dev **devs[DEVICE_LIST_SIZE];
	/* number of items AND index of next empty box */
	size_t count;
};

/*
 *
 *
 *	KPB FMT config set steps:
 *	1. Get dev_ids of module instances from IPC
 *	2. Alloc this kpb module instance on kpb_list_item, save address of where it was allocated
 *	3. Push the address on dev_list.device_list
 *	2. For each dev_id get device handler(module instance)
 *	3. Alloc device handler in modules_list_item, save address of where it was allocated
 *	4. Register this address in fmt.device_list
 *
 *	Pointer structure:
 *
 *		COMP_DEVS
 *		  ^
 *		  |
 *		dev_list.modules_list_item(comp_dev* )
 *		  ^
 *		  |
 *		dev_list.device_list(comp_dev**)
 *		  ^
 *		  |
 *		fmt.device_list(device_list*)
 *
 *
 */

/* KpbFastModeTaskModulesList Namespace */
struct kpb_fmt_dev_list {
	/*! Array of  all module lists to be processed. */
	struct device_list device_list[FAST_MODE_TASK_MAX_LIST_COUNT];
	/* One for each sinkpin. */
	struct comp_dev *kpb_list_item[KPB_MAX_SINK_CNT];
	struct comp_dev *modules_list_item[FAST_MODE_TASK_MAX_MODULES_COUNT];
	struct comp_dev *kpb_mi_ptr;
};

#ifdef UNIT_TEST
void sys_comp_kpb_init(void);
#endif

#endif /* __SOF_AUDIO_KPB_H__ */
