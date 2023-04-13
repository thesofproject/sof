/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Intel Corporation
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#ifndef __SOF_LIB_AMS_HELPERS_H__
#define __SOF_LIB_AMS_HELPERS_H__

#include <sof/lib/ams.h>
#include <sof/lib/ams_msg.h>
#include <stdint.h>

#if CONFIG_AMS

int ams_helper_register_producer(const struct comp_dev *dev,
				 uint32_t *ams_uuid_id,
				 const uint8_t *msg_uuid);

int ams_helper_unregister_producer(const struct comp_dev *dev,
				   uint32_t ams_uuid_id);

int ams_helper_register_consumer(struct comp_dev *dev,
				 uint32_t *ams_uuid_id,
				 const uint8_t *msg_uuid,
				 ams_msg_callback_fn callback);

int ams_helper_unregister_consumer(struct comp_dev *dev,
				   uint32_t ams_uuid_id,
				   ams_msg_callback_fn callback);

void ams_helper_prepare_payload(const struct comp_dev *dev,
				struct ams_message_payload *payload,
				uint32_t ams_uuid_id,
				uint8_t *message,
				size_t message_size);

#endif /* CONFIG_AMS */

#endif /* __SOF_LIB_AMS_HELPERS_H__ */
