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
 * \brief IPC4 asynchronous notifications, GLITCH reports, and FW ready events.
 * \ingroup ipc4_notification_api
 */

#ifndef __IPC4_NOTIFICATION_H__
#define __IPC4_NOTIFICATION_H__

#include <stdint.h>
#include <ipc/header.h>
#include <sof/ipc/msg.h>
#include <sof/compiler_attributes.h>

/**
 * \brief Unsolicited DSP-to-host asynchronous notification types.
 * \ingroup ipc4_notification_api
 */
enum sof_ipc4_notification_type {
	SOF_IPC4_NOTIFY_PHRASE_DETECTED		= 4,  /**< Key phrase detected by wake-on-voice engine. */
	SOF_IPC4_NOTIFY_RESOURCE_EVENT		= 5,  /**< Resource event (underrun, overrun, budget violation). */
	SOF_IPC4_NOTIFY_LOG_BUFFER_STATUS	= 6,  /**< Trace log buffer half-full or aging timer expired. */
	SOF_IPC4_NOTIFY_TIMESTAMP_CAPTURED	= 7,  /**< Audio wallclock timestamp captured. */
	SOF_IPC4_NOTIFY_FW_READY		= 8,  /**< Firmware initialization complete and ready for IPC. */
	SOF_IPC4_FW_AUD_CLASS_RESULT		= 9,  /**< Audio classifier / acoustic event result. */
	SOF_IPC4_EXCEPTION_CAUGHT		= 10, /**< Critical DSP exception or panic caught. */
	SOF_IPC4_MODULE_NOTIFICATION		= 12, /**< Generic module-specific event notification. */
	SOF_IPC4_UAOL_EVENT			= 13, /**< USB Audio Offload event notification. */
	SOF_IPC4_PROBE_DATA_AVAILABLE		= 14, /**< Probe extraction buffer contains recorded audio. */
	SOF_IPC4_WATCHDOG_TIMEOUT		= 15, /**< Watchdog timer timeout interrupt on DSP core. */
	SOF_IPC4_MANAGEMENT_SERVICE		= 16, /**< System management service event notification. */
};

/**
 * \brief Specific resource event types reported by identifiable FW entities.
 * \ingroup ipc4_notification_api
 */
enum sof_ipc4_resource_event_type {
	SOF_IPC4_BUDGET_VIOLATION		= 0,  /**< MCPS budget for module or core exceeded. */
	SOF_IPC4_MIXER_UNDERRUN_DETECTED	= 1,  /**< Underrun detected by mixer/mixin component. */
	SOF_IPC4_STREAM_DATA_SEGMENT		= 2,  /**< Stream data segment completed by gateway. */
	SOF_IPC4_PROCESS_DATA_ERROR		= 3,  /**< Error caught during audio processing. */
	SOF_IPC4_STACK_OVERFLOW			= 4,  /**< Stack overflow detected in module or thread. */
	SOF_IPC4_BUFFERING_MODE_CHANGED		= 5,  /**< Key Phrase Buffer (KPB) buffering mode changed. */
	SOF_IPC4_GATEWAY_UNDERRUN_DETECTED	= 6,  /**< Underrun detected at peripheral DMA gateway. */
	SOF_IPC4_GATEWAY_OVERRUN_DETECTED	= 7,  /**< Overrun detected at peripheral DMA gateway. */
	SOF_IPC4_EDF_DOMAIN_UNSTABLE		= 8,  /**< Data-processing EDF task missed its deadline. */
	SOF_IPC4_WATCHDOG_EVENT			= 9,  /**< Watchdog expiration event. */
	SOF_IPC4_GATEWAY_HIGH_THRES		= 10, /**< IPC gateway reached high watermark threshold. */
	SOF_IPC4_GATEWAY_LOW_THRES		= 11, /**< IPC gateway reached low watermark threshold. */
	SOF_IPC4_I2S_BCE_DETECTED		= 12, /**< Bit Count Error detected on I2S/SSP port. */
	SOF_IPC4_I2S_CLK_STATE_CHANGED		= 13, /**< Clock state change (detected/lost) on I2S port. */
	SOF_IPC4_I2S_SINK_MODE_CHANGED		= 14, /**< I2S sink mode change (started/stopped dropping). */
	SOF_IPC4_I2S_SOURCE_MODE_CHANGED	= 15, /**< I2S source mode change (started/stopped zero-fill). */
	SOF_IPC4_SRE_DRIFT_TOO_HIGH		= 16, /**< Sample Rate Estimator frequency drift exceeded limit. */
	SOF_IPC4_TELEMETRY_DATA_STATUS		= 17, /**< Telemetry buffer exceeded threshold or aged. */
	SOF_IPC4_SNDW_DEBUG_INFO		= 18, /**< SoundWire debug notification (e.g. external VAD). */
	SOF_IPC4_INVALID_RESOURCE_EVENT_TYPE	= 19, /**< Invalid resource event sentinel. */
};

/**
 * \brief Originator resource category firing a resource event.
 * \ingroup ipc4_notification_api
 */
enum sof_ipc4_resource_type {
	SOF_IPC4_MODULE_INSTANCE		= 0, /**< Event originated from a module instance. */
	SOF_IPC4_PIPELINE			= 1, /**< Event originated from a pipeline. */
	SOF_IPC4_GATEWAY			= 2, /**< Event originated from a DMA gateway. */
	SOF_IPC4_EDF_TASK			= 3, /**< Event originated from an EDF DP task. */
	SOF_IPC4_INVALID_RESOURCE_TYPE		= 4, /**< Invalid resource type sentinel. */
};

/** \brief Global notification direction bitmask. */
#define SOF_IPC4_GLB_NOTIFY_DIR_MASK		BIT(29)
/** \brief Reply status mask in primary register. */
#define SOF_IPC4_REPLY_STATUS_MASK		0xFFFFFF
/** \brief Bit shift for notification type in primary register. */
#define SOF_IPC4_GLB_NOTIFY_TYPE_SHIFT		16
/** \brief Bit shift for message type in primary register. */
#define SOF_IPC4_GLB_NOTIFY_MSG_TYPE_SHIFT	24

/** \brief Pre-computed header word for FW_READY notification. */
#define SOF_IPC4_FW_READY \
		(((SOF_IPC4_NOTIFY_FW_READY) << (SOF_IPC4_GLB_NOTIFY_TYPE_SHIFT)) |\
		((SOF_IPC4_GLB_NOTIFICATION) << (SOF_IPC4_GLB_NOTIFY_MSG_TYPE_SHIFT)))

/** \brief FW_READY extension flag indicating dynamic libraries were restored. */
#define SOF_IPC4_FW_READY_LIB_RESTORED		BIT(15)

/** \brief Helper macro to construct notification primary header DWORD. */
#define SOF_IPC4_NOTIF_HEADER(notif_type) \
		((notif_type) << (SOF_IPC4_GLB_NOTIFY_TYPE_SHIFT) | \
		((SOF_IPC4_GLB_NOTIFICATION) << (SOF_IPC4_GLB_NOTIFY_MSG_TYPE_SHIFT)))

/**
 * \brief Standard 32-bit IPC4 notification primary header word.
 * \ingroup ipc4_notification_api
 */
union ipc4_notification_header {
	uint32_t dat; /**< Raw 32-bit register value. */

	struct {
		uint32_t rsvd0 : 16;       /**< Reserved bits. */
		uint32_t notif_type : 8;   /**< Notification type (enum sof_ipc4_notification_type). */
		uint32_t type : 5;         /**< Opcode: SOF_IPC4_GLB_NOTIFICATION (27). */
		uint32_t rsp : 1;          /**< Message direction: 0 for request/notification. */
		uint32_t msg_tgt : 1;      /**< Message target: 0 for global message. */
		uint32_t _reserved_0 : 1;  /**< Reserved bit. */
	} r; /**< Bitfield layout. */
} __packed __aligned(4);

/**
 * \brief This notification is reported by the Detector module upon key phrase detection.
 */
struct ipc4_voice_cmd_notification {
	union {
		uint32_t dat;

		struct {
			uint32_t word_id : 16;     /**< ID of detected keyword. */
			uint32_t notif_type : 8;   /**< Notification type (SOF_IPC4_NOTIFY_PHRASE_DETECTED). */
			uint32_t type : 5;         /**< Opcode: SOF_IPC4_GLB_NOTIFICATION. */
			uint32_t rsp : 1;          /**< Direction: 0 for unsolicited notification. */
			uint32_t msg_tgt : 1;      /**< Message target: 0 for global message. */
			uint32_t _reserved_0 : 1;  /**< Reserved bit. */
		} r; /**< Primary bitfields. */
	} primary; /**< Primary 32-bit register union. */

	union {
		uint32_t dat; /**< Raw 32-bit register value. */

		struct {
			uint32_t sv_score : 16;   /**< Final speaker verification score (0..8192). */
			uint32_t rsvd1 : 14;      /**< Reserved bits. */
			uint32_t _reserved_2 : 2; /**< Reserved bits. */
		} r; /**< Extension bitfields. */
	} extension; /**< Extension 32-bit register union. */
} __packed __aligned(4);

/**
 * \brief Watchdog timeout notification structure (SOF_IPC4_WATCHDOG_TIMEOUT).
 * \ingroup ipc4_notification_api
 *
 * Reported by Base FW when a DSP core encounters a watchdog timeout interrupt.
 */
struct ipc4_watchdog_timeout_notification {
	union {
		uint32_t dat; /**< Raw 32-bit register value. */

		struct {
			uint32_t core_id : 4;       /**< ID of core that timed out. */
			uint32_t first_timeout : 1; /**< 1 if initial timeout and crash dump was taken. */
			uint32_t rsvd : 11;         /**< Reserved padding. */
			uint32_t notif_type : 8;    /**< SOF_IPC4_WATCHDOG_TIMEOUT. */
			uint32_t type : 5;          /**< SOF_IPC4_GLB_NOTIFICATION. */
			uint32_t rsp : 1;           /**< 0 for notification. */
			uint32_t msg_tgt : 1;       /**< 0 for global message. */
			uint32_t _hw_rsvd_0 : 1;    /**< Hardware reserved bit. */
		} r; /**< Primary bitfield layout. */
	} primary; /**< Primary register value. */

	union {
		uint32_t dat; /**< Raw 32-bit register value. */

		struct {
			uint32_t rsvd1 : 30;      /**< Reserved padding. */
			uint32_t _hw_rsvd_2 : 2;  /**< Hardware reserved bits. */
		} r; /**< Extension bitfield layout. */
	} extension; /**< Extension register value. */
} __packed __aligned(4);

/**
 * \brief Helper to initialize watchdog timeout notification structure.
 * \ingroup ipc4_notification_api
 * \param notif Pointer to watchdog timeout notification structure to initialize.
 * \param core_id DSP core ID that timed out.
 * \param first_timeout True if this is the first timeout event.
 */
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
 * \brief Data payload for process data error event (SOF_IPC4_PROCESS_DATA_ERROR).
 * \ingroup ipc4_notification_api
 */
struct ipc4_process_data_error_event_data {
	uint32_t error_code; /**< Native error code returned by processing function. */
};

/**
 * \brief Data payload for mixer underrun event (SOF_IPC4_MIXER_UNDERRUN_DETECTED).
 * \ingroup ipc4_notification_api
 */
struct ipc4_mixer_underrun_event_data {
	uint32_t eos_flag;             /**< End-Of-Stream flag (1 if EOS reached). */
	uint32_t data_mixed;          /**< Bytes of data successfully mixed. */
	uint32_t expected_data_mixed; /**< Expected bytes of data to be mixed. */
};

/**
 * \brief Polymorphic payload for generic resource event notification.
 * \ingroup ipc4_notification_api
 */
union ipc4_resource_event_data {
	uint32_t dws[6]; /**< Raw 6-DWORD data array for backwards compatibility. */
	struct ipc4_process_data_error_event_data process_data_error; /**< Process error payload. */
	struct ipc4_mixer_underrun_event_data mixer_underrun;         /**< Mixer underrun payload. */
};

/**
 * \brief Full resource event notification structure sent in Mailbox Window 0.
 * \ingroup ipc4_notification_api
 */
struct ipc4_resource_event_data_notification {
	uint32_t resource_type; /**< Source category (enum sof_ipc4_resource_type). */
	uint32_t resource_id;   /**< Instance identifier of the firing entity. */
	uint32_t event_type;    /**< Event type (enum sof_ipc4_resource_event_type). */
	uint32_t reserved0;     /**< Explicit alignment padding for 64-bit alignment. */
	union ipc4_resource_event_data event_data; /**< Event-specific data union. */
} __packed __aligned(8);

/** \brief Size in bytes of resource event notification data packet. */
#define IPC4_RESOURCE_EVENT_SIZE	sizeof(struct ipc4_resource_event_data_notification)

/**
 * \brief Send process data error notification message to host.
 * \ingroup ipc4_notification_api
 * \param resource_id ID of module instance encountering error.
 * \param error_code Module-specific error code.
 */
void send_process_data_error_notif_msg(uint32_t resource_id, uint32_t error_code);

/**
 * \brief Send Copier gateway xrun (underrun/overrun) notification to host.
 * \ingroup ipc4_notification_api
 * \param pipeline_id Target pipeline identifier.
 * \param dir Stream direction (playback underrun or capture overrun).
 * \return True if dispatched or suppressed by notification mask; false if notification pool was exhausted and caller should retry.
 */
bool send_copier_gateway_xrun_notif_msg(uint32_t pipeline_id, enum sof_ipc_stream_direction dir);

/**
 * \brief Send DMA gateway xrun notification to host.
 * \ingroup ipc4_notification_api
 * \param resource_id Gateway connector node ID.
 * \param dir Stream direction.
 * \return True if dispatched or suppressed by notification mask; false if notification pool was exhausted and caller should retry.
 */
bool send_gateway_xrun_notif_msg(uint32_t resource_id, enum sof_ipc_stream_direction dir);

/**
 * \brief Send mixer underrun notification message to host.
 * \ingroup ipc4_notification_api
 * \param resource_id Target mixer pipeline identifier.
 * \param eos_flag End of stream flag.
 * \param data_mixed Actual bytes mixed.
 * \param expected_data_mixed Expected bytes mixed.
 */
void send_mixer_underrun_notif_msg(uint32_t resource_id, uint32_t eos_flag, uint32_t data_mixed,
				   uint32_t expected_data_mixed);

/**
 * \brief Update host notification enable/disable bitmasks.
 * \ingroup ipc4_notification_api
 * \param ntfy_mask Bitmask of notifications to update.
 * \param enabled_mask Target state (1 = enable, 0 = disable) for selected bits.
 */
void ipc4_update_notification_mask(uint32_t ntfy_mask, uint32_t enabled_mask);

#if defined(__ZEPHYR__) && defined(CONFIG_SOF_FULL_ZEPHYR_APPLICATION)

/**
 * \brief Send generic resource event notification.
 * \ingroup ipc4_notification_api
 * \param resource_id ID of originator resource.
 * \param event_type Event type (enum sof_ipc4_resource_event_type).
 * \param resource_type Resource category (enum sof_ipc4_resource_type).
 * \param data Pointer to event payload.
 * \param data_size Size of event payload in bytes.
 * \return True if dispatched or suppressed by notification mask; false if notification pool was exhausted and caller should retry.
 */
__syscall bool send_resource_notif(uint32_t resource_id, uint32_t event_type,
				   uint32_t resource_type, void *data, uint32_t data_size);
#include <zephyr/syscalls/notification.h>

#else

/**
 * \brief Implementation of send_resource_notif for non-Zephyr builds.
 * \ingroup ipc4_notification_api
 * \param resource_id ID of originator resource.
 * \param event_type Event type (enum sof_ipc4_resource_event_type).
 * \param resource_type Resource category (enum sof_ipc4_resource_type).
 * \param data Pointer to event payload.
 * \param data_size Size of event payload in bytes.
 * \return True if dispatched or suppressed by notification mask; false if notification pool was exhausted and caller should retry.
 */
bool z_impl_send_resource_notif(uint32_t resource_id, uint32_t event_type,
				uint32_t resource_type, void *data, uint32_t data_size);
#define send_resource_notif z_impl_send_resource_notif

#endif

#endif /* __IPC4_NOTIFICATION_H__ */
