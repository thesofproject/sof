/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

/*
 * This file contains structures that are exact copies of an existing ABI used
 * by IOT middleware. They are Intel specific and will be used by one middleware.
 *
 * Some of the structures may contain programming implementations that makes them
 * unsuitable for generic use and general usage.
 *
 * This code is mostly copied "as-is" from existing C++ interface files hence the use of
 * different style in places. The intention is to keep the interface as close as possible to
 * original so it's easier to track changes with IPC host code.
 */

/**
 * \file include/ipc4/notification.h
 * \brief IPC4 notification definitions
 */

#ifndef __IPC4_NOTIFICATION_H__
#define __IPC4_NOTIFICATION_H__

#include <stdint.h>
#include <ipc/header.h>
#include <sof/compiler_attributes.h>

/* ipc4 notification msg */
enum sof_ipc4_notification_type {
	SOF_IPC4_NOTIFY_PHRASE_DETECTED		= 4,
	SOF_IPC4_NOTIFY_RESOURCE_EVENT		= 5,
	SOF_IPC4_NOTIFY_LOG_BUFFER_STATUS	= 6,
	SOF_IPC4_NOTIFY_TIMESTAMP_CAPTURED	= 7,
	SOF_IPC4_NOTIFY_FW_READY		= 8,
	SOF_IPC4_FW_AUD_CLASS_RESULT		= 9,
	SOF_IPC4_EXCEPTION_CAUGHT		= 10,
	SOF_IPC4_MODULE_NOTIFICATION		= 12,
	SOF_IPC4_UAOL_RSVD_		= 13,
	SOF_IPC4_PROBE_DATA_AVAILABLE		= 14,
	SOF_IPC4_WATCHDOG_TIMEOUT		= 15,
	SOF_IPC4_MANAGEMENT_SERVICE		= 16,
};

/**
 * \brief Resource Event Notification provides unified structure for events that may be raised by
 * identifiable entity from inside the FW.
 */
enum sof_ipc4_resource_event_type {
	/* MCPS budget for the module exceeded */
	SOF_IPC4_BUDGET_VIOLATION		= 0,
	/* Underrun detected by the Mixer */
	SOF_IPC4_MIXER_UNDERRUN_DETECTED	= 1,
	/* Stream data segment completed by the gateway */
	SOF_IPC4_STREAM_DATA_SEGMENT		= 2,
	/* Error caught during data processing */
	SOF_IPC4_PROCESS_DATA_ERROR		= 3,
	/* Stack overflow in a module instance */
	SOF_IPC4_STACK_OVERFLOW			= 4,
	/* KPB changed buffering mode */
	SOF_IPC4_BUFFERING_MODE_CHANGED		= 5,
	/* Underrun detected by gateway. */
	SOF_IPC4_GATEWAY_UNDERRUN_DETECTED	= 6,
	/* Overrun detected by gateway */
	SOF_IPC4_GATEWAY_OVERRUN_DETECTED	= 7,
	/* DP task missing the deadline */
	SOF_IPC4_EDF_DOMAIN_UNSTABLE		= 8,
	/* Watchdog notification */
	SOF_IPC4_WATCHDOG_EVENT			= 9,
	/* IPC gateway reached high threshold */
	SOF_IPC4_GATEWAY_HIGH_THRES		= 10,
	/* IPC gateway reached low threshold */
	SOF_IPC4_GATEWAY_LOW_THRES		= 11,
	/* Bit Count Error detected on I2S port */
	SOF_IPC4_I2S_BCE_DETECTED		= 12,
	/* Clock detected/loss on I2S port */
	SOF_IPC4_I2S_CLK_STATE_CHANGED		= 13,
	/* I2S Sink started/stopped dropping data in non-blk mode */
	SOF_IPC4_I2S_SINK_MODE_CHANGED		= 14,
	/* I2S Source started/stopped generating 0's in non-blk mode */
	SOF_IPC4_I2S_SOURCE_MODE_CHANGED	= 15,
	/* Frequency drift exceeded limit in SRE */
	SOF_IPC4_SRE_DRIFT_TOO_HIGH		= 16,
	/* The notification should be sent only once after exceeding threshold or aging timer. */
	SOF_IPC4_TELEMETRY_DATA_STATUS		= 17,
	/* SNDW debug notification e.g. external VAD detected */
	SOF_IPC4_SNDW_DEBUG_INFO		= 18,
	/* Invalid type */
	SOF_IPC4_INVALID_RESORUCE_EVENT_TYPE	= 19,
};

/* Resource Type - source of the event */
enum sof_ipc4_resource_type {
	/* Module instance */
	SOF_IPC4_MODULE_INSTANCE		= 0,
	/* Pipeline */
	SOF_IPC4_PIPELINE			= 1,
	/* Gateway */
	SOF_IPC4_GATEWAY			= 2,
	/* EDF Task*/
	SOF_IPC4_EDF_TASK			= 3,
	/* Invalid type */
	SOF_IPC4_INVALID_RESOURCE_TYPE		= 4,
};

#define SOF_IPC4_GLB_NOTIFY_DIR_MASK		BIT(29)
#define SOF_IPC4_REPLY_STATUS_MASK		0xFFFFFF
#define SOF_IPC4_GLB_NOTIFY_TYPE_SHIFT		16
#define SOF_IPC4_GLB_NOTIFY_MSG_TYPE_SHIFT	24

#define SOF_IPC4_FW_READY \
		(((SOF_IPC4_NOTIFY_FW_READY) << (SOF_IPC4_GLB_NOTIFY_TYPE_SHIFT)) |\
		((SOF_IPC4_GLB_NOTIFICATION) << (SOF_IPC4_GLB_NOTIFY_MSG_TYPE_SHIFT)))

#define SOF_IPC4_NOTIF_HEADER(notif_type) \
		((notif_type) << (SOF_IPC4_GLB_NOTIFY_TYPE_SHIFT) | \
		((SOF_IPC4_GLB_NOTIFICATION) << (SOF_IPC4_GLB_NOTIFY_MSG_TYPE_SHIFT)))

#endif

/**
 * \brief IPC MAJOR 4 notification header. All IPC4 notifications use this header.
 */
union ipc4_notification_header {
	uint32_t dat;

	struct {
		uint32_t rsvd0 : 16;

		/**< Notification::MODULE_EVENT */
		uint32_t notif_type : 8;

		/**< One of Global::Type */
		uint32_t type : 5;

		/**< Msg::MSG_REQUEST */
		uint32_t rsp : 1;

		/**< Msg::FW_GEN_MSG */
		uint32_t msg_tgt : 1;

		uint32_t _reserved_0 : 1;
	} r;
} __packed __aligned(4);

/**
 * \brief This notification is reported by the Detector module upon key phrase detection.
 */
struct ipc4_voice_cmd_notification {
	union {
		uint32_t dat;

		struct {
			/**< ID of detected keyword */
			uint32_t word_id : 16;
			/**< Notification::PHRASE_DETECTED */
			uint32_t notif_type : 8;
			/**< Global::NOTIFICATION */
			uint32_t type : 5;
			/**< Msg::MSG_NOTIFICATION */
			uint32_t rsp : 1;
			/**< Msg::FW_GEN_MSG */
			uint32_t msg_tgt : 1;
			uint32_t _reserved_0 : 1;
		} r;
	} primary;

	union {
		uint32_t dat;

		struct {
			/**< Final speaker verification score in range of 0..8192 */
			uint32_t sv_score : 16;
			uint32_t rsvd1 : 14;
			uint32_t _reserved_2 : 2;
		} r;
	} extension;
} __packed __aligned(4);

/**
 * \brief This notification is reported by the Base FW when DSP core receive WDT timeout interrupt.
 */
struct ipc4_watchdog_timeout_notification {
	union {
		uint32_t dat;

		struct {
			/* ID of a core that timeouted. */
			uint32_t core_id : 4;
			/* Indicates that it was first timeout and crash dump was done */
			uint32_t first_timeout : 1;
			uint32_t rsvd : 11;
			/* Notification::WATCHDOG_TIMEOUT */
			uint32_t notif_type : 8;
			/* Global::NOTIFICATION */
			uint32_t type : 5;
			/* Msg::MSG_NOTIFICATION (0) */
			uint32_t rsp : 1;
			/* Msg::FW_GEN_MSG */
			uint32_t msg_tgt : 1;
			uint32_t _hw_rsvd_0 : 1;
		} r;
	} primary;

	union {
		uint32_t dat;

		struct {
			uint32_t rsvd1 : 30;
			uint32_t _hw_rsvd_2 : 2;
		} r;
	} extension;
} __packed __aligned(4);

static inline void ipc4_notification_watchdog_init(struct ipc4_watchdog_timeout_notification *notif,
						   uint32_t core_id, bool first_timeout)
{
	notif->primary.dat = 0;
	notif->extension.dat = 0;

	/* ID of a core that timeouted. */
	notif->primary.r.core_id = core_id;

	/* Indicates that it was first timeout and crash dump was done */
	notif->primary.r.first_timeout = first_timeout;

	notif->primary.r.notif_type = SOF_IPC4_WATCHDOG_TIMEOUT;
	notif->primary.r.type = SOF_IPC4_GLB_NOTIFICATION;
	notif->primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	notif->primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
}

/**
 * \brief Input data payload is reserved field in parent technical spec which can be easily
 * extendable if needed by specific resource event types in the future. For backward compatibility
 * the size of this structure is 6 dw.
 */
union ipc4_resource_event_data {
	/* Raw data */
	uint32_t dws[6];
};

struct ipc4_resource_event_data_notification {
	/* Type of originator (see sof_ipc4_resource_type) */
	uint32_t resource_type;
	/* ID of resource firing event */
	uint32_t resource_id;
	/* Type of fired event (see sof_ipc4_resource_event_type enum) */
	uint32_t event_type;
	/* Explicit alignment as ipc4_resource_event_data contains 64bit fields and needs
	 * alignment
	 */
	uint32_t reserved0;
	/* Detailed event data */
	union ipc4_resource_event_data event_data;
} __packed __aligned(8);

#if CONFIG_XRUN_NOTIFICATIONS_ENABLE
void xrun_notif_msg_init(struct ipc_msg *msg_xrun, uint32_t resource_id, uint32_t event_type);
#endif
