/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef MODULE_MODULE_SYSTEM_SERVICE_H
#define MODULE_MODULE_SYSTEM_SERVICE_H

#include <stdint.h>
#include <stddef.h>

#include "logger.h"
#include "../iadk/adsp_error_code.h"

#ifdef __XTENSA__
	#define RESTRICT __restrict
#else
	#define RESTRICT
#endif

/*! This struct defines the obfuscating type for notifications. */
struct notification_handle;

/* Defines parameters used by ADSP system during notification creation. */
struct notification_params {
	uint32_t type;              /*!< Notification type */
	uint16_t user_val_1;        /*!< 16 bits user value available directly in IPC header
				     * for some notifications
				     */
	uint32_t user_val_2;        /*!< 30 bits user value available directly in IPC header
				     * for some notifications
				     */
	uint32_t max_payload_size;  /*!< Data size of payload (NotificationCreate updates this
				     * value to max possible payload size)
				     */
	uint8_t *payload;           /*!< Pointer on the payload */
};

/* Defines parameters used by ADSP system during Module Event notification creation. */
struct module_event_notification {
	uint32_t module_instance_id; /*!< Module ID (MS word) + Module Instance ID (LS word) */
	uint32_t event_id;           /*!< Module specific event ID. */
	uint32_t event_data_size;    /*!< Size of event_data array in bytes. May be set to 0
				      * in case there is no data.
				      */
	uint32_t event_data[];       /*!< Optional event data (size set to 0 as it is optional
				      * data)
				      */
};

/* Defines notification targets supported by ADSP system
 * FW defines only two notification targets, HOST and ISH (Integrated Sensor Hub).
 */
enum notification_target {
	NOTIFICATION_TARGET_DSP_TO_HOST = 1,	/* Notification target is HOST */
	NOTIFICATION_TARGET_DSP_TO_ISH = 2	/* Notification target is ISH */
};

/* Defines notification types supported by ADSP system
 * FW use reserved first 20 positions describing types of notifications.
 */
enum notification_type {
	/* corresponding to PHRASE_DETECTED notification */
	NOTIFICATION_TYPE_VOICE_COMMAND_NOTIFICATION = 4,

	/* corresponding to FW_AUD_CLASS_RESULT notification */
	NOTIFICATION_TYPE_AUDIO_CLASSIFIER_RESULTS = 9,

	/* corresponding to MODULE_NOTIFICATION notification */
	NOTIFICATION_TYPE_MODULE_EVENT_NOTIFICATION = 12,
};

/* Defines extended interfaces for IADK modules */
enum interface_id {
	INTERFACE_ID_GNA = 0x1000,			/* Reserved for ADSP system */
	INTERFACE_ID_INFERENCE_SERVICE = 0x1001,	/* See InferenceServiceInterface */
	INTERFACE_ID_SDCA = 0x1002,			/* See SdcaInterface */
	INTERFACE_ID_ASYNC_MESSAGE_SERVICE = 0x1003,	/* See AsyncMessageInterface */
	INTERFACE_ID_AM_SERVICE = 0x1005,		/* Reserved for ADSP system */
	INTERFACE_ID_KPB_SERVICE = 0x1006		/* See KpbInterface */
};

/* sub interface definition.
 * This type may contain generic interface properties like id or struct size if needed.
 */
struct system_service_iface;

struct system_service {
	void (*log_message)(enum log_priority log_priority, uint32_t log_entry,
			    struct log_handle const *log_handle, uint32_t param1,
			    uint32_t param2, uint32_t param3, uint32_t param4);

	AdspErrorCode (*safe_memcpy)(void *RESTRICT dst, size_t maxlen,
				     const void *RESTRICT src, size_t len);

	AdspErrorCode (*safe_memmove)(void *dst, size_t maxlen,
				      const void *src, size_t len);

	void* (*vec_memset)(void *dst, int c, size_t len);

	AdspErrorCode (*notification_create)(struct notification_params *params,
					     uint8_t *notification_buffer,
					     uint32_t notification_buffer_size,
					     struct notification_handle **handle);
	AdspErrorCode (*notification_send)(enum notification_target notification_target,
					   struct notification_handle *message,
					   uint32_t actual_payload_size);

	AdspErrorCode (*get_interface)(enum interface_id id, struct system_service_iface **iface);
};
#endif /* MODULE_MODULE_SYSTEM_SERVICE_H */
