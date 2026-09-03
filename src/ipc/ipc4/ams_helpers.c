// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#include <ipc4/ams_helpers.h>
#include <sof/audio/component.h>

#if CONFIG_AMS

int ams_helper_register_producer(const struct comp_dev *dev,
				 uint32_t *ams_uuid_id,
				 const uint8_t *msg_uuid)
{
	uint16_t mod_id = IPC4_MOD_ID(dev_comp_id(dev));
	uint16_t inst_id = IPC4_INST_ID(dev_comp_id(dev));
	int ret;

	ret = ams_get_message_type_id(msg_uuid, ams_uuid_id);
	if (ret)
		return ret;

	return ams_register_producer(*ams_uuid_id, mod_id, inst_id);
}

int ams_helper_unregister_producer(const struct comp_dev *dev,
				   uint32_t ams_uuid_id)
{
	uint16_t mod_id = IPC4_MOD_ID(dev_comp_id(dev));
	uint16_t inst_id = IPC4_INST_ID(dev_comp_id(dev));

	return ams_unregister_producer(ams_uuid_id, mod_id, inst_id);
}

int ams_helper_register_consumer(struct comp_dev *dev,
				 uint32_t *ams_uuid_id,
				 const uint8_t *msg_uuid,
				 ams_msg_callback_fn callback)
{
	uint16_t mod_id = IPC4_MOD_ID(dev_comp_id(dev));
	uint16_t inst_id = IPC4_INST_ID(dev_comp_id(dev));
	int ret;

	ret = ams_get_message_type_id(msg_uuid, ams_uuid_id);
	if (ret)
		return ret;

	return ams_register_consumer(*ams_uuid_id, mod_id, inst_id, callback, dev);
}

int ams_helper_unregister_consumer(struct comp_dev *dev,
				   uint32_t ams_uuid_id,
				   ams_msg_callback_fn callback)
{
	uint16_t mod_id = IPC4_MOD_ID(dev_comp_id(dev));
	uint16_t inst_id = IPC4_INST_ID(dev_comp_id(dev));

	return ams_unregister_consumer(ams_uuid_id, mod_id, inst_id, callback);
}

void ams_helper_prepare_payload(const struct comp_dev *dev,
				struct ams_message_payload *payload,
				uint32_t ams_uuid_id,
				uint8_t *message,
				size_t message_size)
{
	uint16_t mod_id = IPC4_MOD_ID(dev_comp_id(dev));
	uint16_t inst_id = IPC4_INST_ID(dev_comp_id(dev));

	payload->message_type_id = ams_uuid_id;
	payload->producer_module_id = mod_id;
	payload->producer_instance_id = inst_id;
	payload->message_length = message_size;
	payload->message = message;
}

#endif /* CONFIG_AMS */
