/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */
/*! \file native_system_service.h */
#ifndef NATIVE_SYSTEM_SERVICE_H
#define NATIVE_SYSTEM_SERVICE_H

#include <stdint.h>

#include "adsp_stddef.h"
#include <module/iadk/adsp_error_code.h>

/*! \brief This struct defines the obfuscating type for notifications. */
typedef struct _adsp_notification_handle {} *adsp_notification_handle;

/*! \brief Defines parameters used by ADSP system during notification creation. */
typedef struct _notification_params {
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
} notification_params;

/*! \brief Defines parameters used by ADSP system during Module Event notification creation. */
typedef struct _module_event_notification {
	uint32_t module_instance_id; /*!< Module ID (MS word) + Module Instance ID (LS word) */
	uint32_t event_id;           /*!< Module specific event ID. */
	uint32_t event_data_size;    /*!< Size of event_data array in bytes. May be set to 0
				      * in case there is no data.
				      */
	uint32_t event_data[];       /*!< Optional event data (size set to 0 as it is optional
				      * data)
				      */
} module_event_notification;

/*! \brief Defines notification targets supported by ADSP system
 * Legacy FW defines only two notification targets, HOST and ISH (Integrated Sensor Hub).
 */
typedef enum _notification_target {
	NOTIFICATION_TARGET_DSP_TO_HOST = 1,  /*!< Notification target is HOST */
	NOTIFICATION_TARGET_DSP_TO_ISH = 2    /*!< Notification target is ISH */
} adsp_notification_target;

/*! \brief Defines notification types supported by ADSP system
 * Legacy FW uses reserves first 20 positions descibing types of notifications.
 */
typedef enum _notification_type {
	NOTIFICATION_TYPE_VOICE_COMMAND_NOTIFICATION = 4,	/*!< intel_adsp define
								 * corresponding to PHRASE_DETECTED
								 * notification
								 */
	NOTIFICATION_TYPE_AUDIO_CLASSIFIER_RESULTS = 9,		/*!< intel_adsp define
								 * corresponding to
								 * FW_AUD_CLASS_RESULT notification
								 */
	NOTIFICATION_TYPE_MODULE_EVENT_NOTIFICATION = 12,	/*!< intel_adsp define
								 * corresponding to
								 * MODULE_NOTIFICATION notification
								 */
} notification_type;

/*! \brief Defines extended interfaces for IADK modules*/
typedef enum _adsp_iface_id {
	INTERFACE_ID_GNA = 0x1000,			/*!< Reserved for ADSP system */
	INTERFACE_ID_INFERENCE_SERVICE = 0x1001,	/*!< See InferenceServiceInterface */
	INTERFACE_ID_SDCA = 0x1002,			/*!< See SdcaInterface */
	INTERFACE_ID_ASYNC_MESSAGE_SERVICE = 0x1003,	/*!< See AsyncMessageInterface */
	INTERFACE_ID_AM_SERVICE = 0x1005,		/*!< Reserved for ADSP system */
	INTERFACE_ID_KPB_SERVICE = 0x1006		/*!< See KpbInterface */
} adsp_iface_id;

/*! \brief sub interface definition.
 * This type may contain generic interface properties like id or struct size if needed.
 */
typedef struct _system_service_iface {} system_service_iface;

/*! \brief Defines prototype of the "GetInterface" function
 *
 * \param id service id
 * \param iface pointer to retrieved interface
 * \return error if service not supported
 */

struct native_system_service_api {
	void (*log_message)(AdspLogPriority log_priority, uint32_t log_entry,
			    AdspLogHandle const *log_handle, uint32_t param1,
			    uint32_t param2, uint32_t param3, uint32_t param4);

	AdspErrorCode (*safe_memcpy)(void *RESTRICT dst, size_t maxlen,
				     const void *RESTRICT src, size_t len);

	AdspErrorCode (*safe_memmove)(void *dst, size_t maxlen,
				      const void *src, size_t len);

	void* (*vec_memset)(void *dst, int c, size_t len);

	AdspErrorCode (*notification_create)(notification_params *params,
					     uint8_t *notification_buffer,
					     uint32_t notification_buffer_size,
					     adsp_notification_handle *handle);
	AdspErrorCode (*notification_send)(adsp_notification_target notification_target,
					   adsp_notification_handle message,
					   uint32_t actual_payload_size);

	AdspErrorCode (*get_interface)(adsp_iface_id id, system_service_iface **iface);
};
#endif /*NATIVE_SYSTEM_SERVICE_H*/
