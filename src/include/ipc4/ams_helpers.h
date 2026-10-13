/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Intel Corporation
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

/**
 * \file include/ipc4/ams_helpers.h
 * \brief Audio Management System (AMS) message producer and consumer helpers.
 * \ingroup ipc4_envelope_api
 */

#ifndef __SOF_LIB_AMS_HELPERS_H__
#define __SOF_LIB_AMS_HELPERS_H__

#include <sof/lib/ams.h>
#include <sof/lib/ams_msg.h>
#include <stdint.h>

#if CONFIG_AMS

/**
 * \brief Register a component device as an AMS message producer.
 * \ingroup ipc4_envelope_api
 * @param[in] dev Pointer to the component device.
 * @param[out] ams_uuid_id Receives the allocated AMS UUID registration identifier.
 * @param[in] msg_uuid 16-byte UUID of the message type.
 * @return 0 on success, or negative error code on registration failure.
 */
int ams_helper_register_producer(const struct comp_dev *dev,
				 uint32_t *ams_uuid_id,
				 const uint8_t *msg_uuid);

/**
 * \brief Unregister a component device as an AMS message producer.
 * \ingroup ipc4_envelope_api
 * @param[in] dev Pointer to the component device.
 * @param[in] ams_uuid_id AMS UUID identifier previously allocated by registration.
 * @return 0 on success, or negative error code on failure.
 */
int ams_helper_unregister_producer(const struct comp_dev *dev,
				   uint32_t ams_uuid_id);

/**
 * \brief Register a component device as an AMS message consumer with a receive callback.
 * \ingroup ipc4_envelope_api
 * @param[in] dev Pointer to the component device.
 * @param[out] ams_uuid_id Receives the allocated AMS UUID registration identifier.
 * @param[in] msg_uuid 16-byte UUID of the message type to subscribe to.
 * @param[in] callback Function pointer invoked when a matching AMS message arrives.
 * @return 0 on success, or negative error code on registration failure.
 */
int ams_helper_register_consumer(struct comp_dev *dev,
				 uint32_t *ams_uuid_id,
				 const uint8_t *msg_uuid,
				 ams_msg_callback_fn callback);

/**
 * \brief Unregister a component device as an AMS message consumer.
 * \ingroup ipc4_envelope_api
 * @param[in] dev Pointer to the component device.
 * @param[in] ams_uuid_id AMS UUID registration identifier.
 * @param[in] callback Callback function pointer to remove.
 * @return 0 on success, or negative error code on failure.
 */
int ams_helper_unregister_consumer(struct comp_dev *dev,
				   uint32_t ams_uuid_id,
				   ams_msg_callback_fn callback);

/**
 * \brief Format and prepare an AMS message payload for transmission.
 * \ingroup ipc4_envelope_api
 * @param[in] dev Pointer to the originator component device.
 * @param[out] payload Pointer to the AMS message payload structure to initialize.
 * @param[in] ams_uuid_id Originator AMS UUID registration identifier.
 * @param[in] message Raw message buffer pointer.
 * @param[in] message_size Size of the message buffer in bytes.
 */
void ams_helper_prepare_payload(const struct comp_dev *dev,
				struct ams_message_payload *payload,
				uint32_t ams_uuid_id,
				uint8_t *message,
				size_t message_size);

#endif /* CONFIG_AMS */

#endif /* __SOF_LIB_AMS_HELPERS_H__ */
