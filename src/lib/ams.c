// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2023 Intel Corporation
 *
 * Author: Krzysztof Frydryk <krzysztofx.frydryk@intel.com>
 */

#include <sof/lib/memory.h>
#include <sof/lib/cpu.h>
#include <rtos/interrupt.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <rtos/task.h>
#include <sof/ipc/topology.h>
#include <rtos/idc.h>
#include <rtos/alloc.h>
#include <sof/lib/memory.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sof/lib/ams.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(ams, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(ams);

DECLARE_TR_CTX(ams_tr, SOF_UUID(ams_uuid), LOG_LEVEL_INFO);

static struct ams_context ctx[CONFIG_CORE_COUNT];

static struct ams_shared_context __sparse_cache *ams_acquire(struct ams_shared_context *shared)
{
	struct coherent __sparse_cache *c = coherent_acquire(&shared->c,
								    sizeof(*shared));

	return attr_container_of(c, struct ams_shared_context __sparse_cache,
					c, __sparse_cache);
}

static void ams_release(struct ams_shared_context __sparse_cache *shared)
{
	coherent_release(&shared->c, sizeof(*shared));
}

static struct uuid_idx __sparse_cache *ams_find_uuid_entry_by_uuid(struct ams_shared_context __sparse_cache *ctx_shared,
								   uint8_t const *uuid)
{
	unsigned int index;
	struct uuid_idx __sparse_cache *uuid_table = ctx_shared->uuid_table;

	if (!uuid)
		return NULL;

	/* try to find existing entry */
	for (index = 0; index < AMS_SERVICE_UUID_TABLE_SIZE; index++) {
		if (memcmp((__sparse_force void *)uuid_table[index].message_uuid,
			   uuid, UUID_SIZE) == 0)
			return &uuid_table[index];
	}

	/* and add new one if needed */
	for (index = 0; index < AMS_SERVICE_UUID_TABLE_SIZE; index++) {
		if (uuid_table[index].message_type_id == AMS_INVALID_MSG_TYPE) {
			int ec = memcpy_s((__sparse_force void *)uuid_table[index].message_uuid,
					  sizeof(uuid_table[index].message_uuid),
					  uuid, UUID_SIZE);
			if (ec != 0) {
				tr_err(&ams_tr, "Failed to create UUID entry: %u", index);
				return NULL;
			}

			uuid_table[index].message_type_id = ++ctx_shared->last_used_msg_id;
			return &uuid_table[index];
		}
	}

	tr_err(&ams_tr, "No space to create UUID entry");
	return NULL;
}

int ams_get_message_type_id(const uint8_t *message_uuid,
			    uint32_t *message_type_id)
{
	struct async_message_service *ams = *arch_ams_get();
	struct uuid_idx __sparse_cache *uuid_entry;
	struct ams_shared_context __sparse_cache *shared_c;

	if (!ams->ams_context)
		return -EINVAL;

	*message_type_id = AMS_INVALID_MSG_TYPE;

	shared_c = ams_acquire(ams->ams_context->shared);

	uuid_entry = ams_find_uuid_entry_by_uuid(shared_c, message_uuid);
	if (!uuid_entry) {
		ams_release(shared_c);
		return -EINVAL;
	}

	*message_type_id = uuid_entry->message_type_id;
	ams_release(shared_c);

	return 0;
}

static int ams_find_uuid_index_by_msg_type_id(struct ams_shared_context __sparse_cache *ctx_shared,
					      uint32_t const message_type_id)
{
	struct uuid_idx __sparse_cache *iter;

	if (message_type_id == AMS_INVALID_MSG_TYPE)
		return -EINVAL;

	for (int i = 0; i < AMS_SERVICE_UUID_TABLE_SIZE; i++) {
		iter = &ctx_shared->uuid_table[i];

		/* we got the id */
		if (message_type_id == iter->message_type_id)
			return i;
	}

	return -ENOENT;
}

int ams_register_producer(uint32_t message_type_id,
			  uint16_t module_id,
			  uint16_t instance_id)
{
	struct async_message_service *ams = *arch_ams_get();
	struct ams_producer __sparse_cache *producer_table;
	struct ams_shared_context __sparse_cache *shared_c;
	int idx;
	int err = -EINVAL;

	if (!ams->ams_context)
		return -EINVAL;

	shared_c = ams_acquire(ams->ams_context->shared);

	idx = ams_find_uuid_index_by_msg_type_id(shared_c, message_type_id);
	if (idx < 0) {
		ams_release(shared_c);
		return -EINVAL;
	}

	producer_table = shared_c->producer_table;
	for (int iter = 0; iter < AMS_ROUTING_TABLE_SIZE; iter++) {
		/* Search for first invalid entry */
		if (producer_table[iter].message_type_id == AMS_INVALID_MSG_TYPE) {
			producer_table[iter].message_type_id = message_type_id;
			producer_table[iter].producer_module_id = module_id;
			producer_table[iter].producer_instance_id = instance_id;

			/* Exit loop since we added new entry */
			err = 0;
			break;
		}
	}

	ams_release(shared_c);
	return err;
}

int ams_unregister_producer(uint32_t message_type_id,
			    uint16_t module_id,
			    uint16_t instance_id)
{
	struct async_message_service *ams = *arch_ams_get();
	struct ams_producer __sparse_cache *producer_table;
	struct ams_shared_context __sparse_cache *shared_c;
	int idx;
	int err = -EINVAL;

	if (!ams->ams_context)
		return -EINVAL;

	shared_c = ams_acquire(ams->ams_context->shared);

	idx = ams_find_uuid_index_by_msg_type_id(shared_c, message_type_id);
	if (idx < 0) {
		ams_release(shared_c);
		return -EINVAL;
	}

	producer_table = shared_c->producer_table;
	for (int iter = 0; iter < AMS_ROUTING_TABLE_SIZE; iter++) {
		if ((producer_table[iter].message_type_id == message_type_id) &&
		    (producer_table[iter].producer_instance_id == instance_id) &&
		    (producer_table[iter].producer_module_id == module_id)) {
			producer_table[iter].message_type_id = AMS_INVALID_MSG_TYPE;

			/* Exit loop since we added new entry */
			err = 0;
			break;
		}
	}

	ams_release(shared_c);
	return err;
}

int ams_register_consumer(uint32_t message_type_id,
			  uint16_t module_id,
			  uint16_t instance_id,
			  ams_msg_callback_fn function,
			  void *ctx)
{
	struct async_message_service *ams = *arch_ams_get();
	struct ams_consumer_entry __sparse_cache *routing_table;
	struct ams_shared_context __sparse_cache *shared_c;
	int err = -EINVAL;

	if (!ams->ams_context || !function)
		return -EINVAL;

	shared_c = ams_acquire(ams->ams_context->shared);

	routing_table = shared_c->rt_table;
	for (int iter = 0; iter < AMS_ROUTING_TABLE_SIZE; iter++) {
		/* Search for first invalid entry */
		if (routing_table[iter].message_type_id == AMS_INVALID_MSG_TYPE) {
			/* Add entry to routing table for local service */
			routing_table[iter].consumer_callback = function;
			routing_table[iter].message_type_id = message_type_id;
			routing_table[iter].consumer_instance_id = instance_id;
			routing_table[iter].consumer_module_id = module_id;
			routing_table[iter].consumer_core_id = cpu_get_id();
			routing_table[iter].ctx = ctx;

			/* Exit loop since we added new entry */
			err = 0;
			break;
		}
	}

	ams_release(shared_c);
	return err;
}

int ams_unregister_consumer(uint32_t message_type_id,
			    uint16_t module_id,
			    uint16_t instance_id,
			    ams_msg_callback_fn function)
{
	struct async_message_service *ams = *arch_ams_get();
	struct ams_consumer_entry __sparse_cache *routing_table;
	struct ams_shared_context __sparse_cache *shared_c;
	int err = -EINVAL;

	if (!ams->ams_context)
		return -EINVAL;

	shared_c = ams_acquire(ams->ams_context->shared);
	routing_table = shared_c->rt_table;
	for (int iter = 0; iter < AMS_ROUTING_TABLE_SIZE; iter++) {
		/* Search for required entry */
		if ((routing_table[iter].message_type_id == message_type_id) &&
		    (routing_table[iter].consumer_module_id == module_id) &&
		    (routing_table[iter].consumer_instance_id == instance_id) &&
		    (routing_table[iter].consumer_callback == function)) {
			/* Remove this entry from routing table */
			routing_table[iter].message_type_id = AMS_INVALID_MSG_TYPE;
			routing_table[iter].consumer_callback = NULL;

			/* Exit loop since we removed entry */
			err = 0;
			break;
		}
	}

	ams_release(shared_c);
	return err;
}

static uint32_t ams_push_slot(struct ams_shared_context __sparse_cache *ctx_shared,
			      const struct ams_message_payload *msg,
			      uint16_t module_id, uint16_t instance_id)
{
	int err;

	for (uint32_t i = 0; i < ARRAY_SIZE(ctx_shared->slots); ++i) {
		if (ctx_shared->slot_uses[i] == 0) {
			err = memcpy_s((__sparse_force void *)ctx_shared->slots[i].u.msg_raw,
				       sizeof(ctx_shared->slots[i].u.msg_raw),
				       msg, AMS_MESSAGE_SIZE(msg));

			if (err != 0)
				return AMS_INVALID_SLOT;

			ctx_shared->slots[i].module_id = module_id;
			ctx_shared->slots[i].instance_id = instance_id;
			ctx_shared->slot_done[i] = 0;

			return i;
		}
	}

	return AMS_INVALID_SLOT;
}

static int ams_get_ixc_route_to_target(int source_core, int target_core)
{
	if (source_core >= CONFIG_CORE_COUNT || target_core >= CONFIG_CORE_COUNT)
		return -EINVAL;
	/* core 0 can target any core */
	if (source_core == PLATFORM_PRIMARY_CORE_ID)
		return target_core;
	/* other cores must proxy through main core */
	return source_core == target_core ? target_core : PLATFORM_PRIMARY_CORE_ID;
}

static int send_message_over_ixc(struct async_message_service *ams, uint32_t slot,
				 struct ams_consumer_entry *target)
{
	if (!target)
		return -EINVAL;

	int ixc_route = ams_get_ixc_route_to_target(cpu_get_id(),
							 target->consumer_core_id);

	struct idc_msg ams_request = {
		.header = IDC_MSG_AMS | slot,
		.extension = IDC_MSG_AMS_EXT,
		.core = ixc_route,
		.size = 0,
		.payload = NULL};

	/* send IDC message */
	return idc_send_msg(&ams_request, IDC_NON_BLOCKING);
}

static int ams_send_over_ixc(struct async_message_service *ams, uint32_t slot,
			     struct ams_consumer_entry *target)
{
#if CONFIG_SMP
	return send_message_over_ixc(ams, slot, target);
#else
	return -EINVAL;
#endif
}

static int ams_message_send_internal(struct async_message_service *ams,
				     const struct ams_message_payload *const ams_message_payload,
				     uint16_t module_id, uint16_t instance_id,
				     uint32_t incoming_slot)
{
	bool found_any = false;
	bool incoming = (incoming_slot != AMS_INVALID_SLOT);
	struct ams_consumer_entry __sparse_cache *routing_table;
	struct ams_shared_context __sparse_cache *shared_c;
	uint32_t forwarded = 0;
	uint32_t slot;
	struct ams_consumer_entry ams_target;
	int ixc_route;
	int cpu_id;
	int err = 0;

	if (!ams->ams_context || !ams_message_payload)
		return -EINVAL;

	shared_c = ams_acquire(ams->ams_context->shared);
	cpu_id = cpu_get_id();

	if (incoming)
		shared_c->slot_done[incoming_slot] |= BIT(cpu_id);

	routing_table = shared_c->rt_table;

	for (int iter = 0; iter < AMS_ROUTING_TABLE_SIZE; iter++) {
		slot = AMS_INVALID_SLOT;

		/* Search for required entry */
		if (routing_table[iter].message_type_id != ams_message_payload->message_type_id)
			continue;

		/* check if we want to limit to specific module* */
		if (module_id != AMS_ANY_ID && instance_id != AMS_ANY_ID) {
			if (routing_table[iter].consumer_module_id != module_id ||
			    routing_table[iter].consumer_instance_id != instance_id) {
				continue;
			}
		}

		found_any = true;
		ams_target = routing_table[iter];
		ixc_route = ams_get_ixc_route_to_target(cpu_id,
							ams_target.consumer_core_id);

		if (ixc_route == cpu_id) {
			/* we are on target core already */
			/* release lock here, callback are NOT supposed to change routing_table */
			ams_release(shared_c);

			ams_target.consumer_callback(ams_message_payload, ams_target.ctx);
			err = 0;
		} else {
			/* we have to go through idc */
			if (incoming) {
				/* if bit is set we are forwarding it again */
				if (shared_c->slot_done[incoming_slot] & BIT(ams_target.consumer_core_id)) {
					/* slot was already processed for that core, skip it */
					continue;
				}
			} else {
				slot = ams_push_slot(shared_c,
						     ams_message_payload, module_id,
						     instance_id);
				if (slot == AMS_INVALID_SLOT) {
					ams_release(shared_c);
					return -EINVAL;
				}
			}
			if ((forwarded & BIT(ams_target.consumer_core_id)) == 0) {
				/* bump uses count, mark current as processed already */
				if (slot != AMS_INVALID_SLOT) {
					shared_c->slot_uses[slot]++;
					shared_c->slot_done[slot] |= BIT(cpu_id);
				}

				/* release lock here, so other core can acquire it again */
				ams_release(shared_c);

				if (slot != AMS_INVALID_SLOT) {
					forwarded |= BIT(ams_target.consumer_core_id);
					err = ams_send_over_ixc(ams, slot, &ams_target);
					if (err != 0) {
						/* idc not sent, update slot refs locally */
						shared_c = ams_acquire(ams->ams_context->shared);
						shared_c->slot_uses[slot]--;
						shared_c->slot_done[slot] |= BIT(ams_target.consumer_core_id);
						ams_release(shared_c);
					}
				}
			} else {
				/* message already forwarded, nothing to do here */
				ams_release(shared_c);
			}
		}

		/* acquire shared context lock again */
		shared_c = ams_acquire(ams->ams_context->shared);
	}

	if (incoming)
		shared_c->slot_uses[incoming_slot]--;

	ams_release(shared_c);

	if (!found_any)
		tr_err(&ams_tr, "No entries found!");

	return err;
}

int ams_send(const struct ams_message_payload *const ams_message_payload)
{
	struct async_message_service *ams = *arch_ams_get();

	return ams_message_send_internal(ams, ams_message_payload, AMS_ANY_ID, AMS_ANY_ID,
					 AMS_INVALID_SLOT);
}

int ams_message_send_mi(struct async_message_service *ams,
			const struct ams_message_payload *const ams_message_payload,
			uint16_t target_module, uint16_t target_instance)
{
	return ams_message_send_internal(ams, ams_message_payload, target_module,
					 target_instance, AMS_INVALID_SLOT);
}

int ams_send_mi(const struct ams_message_payload *const ams_message_payload,
		uint16_t module_id, uint16_t instance_id)
{
	struct async_message_service *ams = *arch_ams_get();

	return ams_message_send_mi(ams, ams_message_payload, module_id, instance_id);
}

static int ams_process_slot(struct async_message_service *ams, uint32_t slot)
{
	struct ams_shared_context __sparse_cache *shared_c;
	struct ams_message_payload msg;
	uint16_t module_id;
	uint16_t instance_id;

	shared_c = ams_acquire(ams->ams_context->shared);

	msg = shared_c->slots[slot].u.msg;
	module_id = shared_c->slots[slot].module_id;
	instance_id = shared_c->slots[slot].instance_id;

	ams_release(shared_c);
	tr_info(&ams_tr, "ams_process_slot slot %d msg %d from 0x%08x",
		slot, msg.message_type_id,
		msg.producer_module_id << 16 | msg.producer_instance_id);

	return ams_message_send_internal(ams, &msg, module_id, instance_id, slot);
}

#if CONFIG_SMP

static void ams_task_add_slot_to_process(struct ams_task *ams_task, uint32_t slot)
{
	int flags;

	irq_local_disable(flags);
	ams_task->pending_slots |= BIT(slot);
	irq_local_enable(flags);
}

int process_incoming_message(uint32_t slot)
{
	struct async_message_service *ams = *arch_ams_get();
	struct ams_task *task = &ams->ams_task;

	ams_task_add_slot_to_process(task, slot);

	return schedule_task(&task->ams_task, 0, 10000);
}

#endif /* CONFIG_SMP */

/* ams task */

static enum task_state process_message(void *arg)
{
	struct ams_task *ams_task = arg;
	uint32_t slot;
	int flags;

	if (ams_task->pending_slots == 0) {
		tr_err(&ams_tr, "Could not process message! Skipping.");
		return SOF_TASK_STATE_COMPLETED;
	}

	slot = 31 - clz(ams_task->pending_slots);

	ams_process_slot(ams_task->ams, slot);

	/* only done on main core, irq disabling is enough */
	irq_local_disable(flags);
	ams_task->pending_slots &= ~BIT(slot);
	irq_local_enable(flags);
	schedule_task_cancel(&ams_task->ams_task);

	return SOF_TASK_STATE_COMPLETED;
}

static int ams_task_init(void)
{
	int ret;
	struct async_message_service *ams = *arch_ams_get();
	struct ams_task *task = &ams->ams_task;

	task->ams = ams;

	ret = schedule_task_init_ll(&task->ams_task, SOF_UUID(ams_uuid), SOF_SCHEDULE_LL_TIMER,
				    SOF_TASK_PRI_MED, process_message, &ams->ams_task, cpu_get_id(), 0);
	if (ret)
		tr_err(&ams_tr, "Could not init AMS task!");

	return ret;
}

static int ams_create_shared_context(struct ams_shared_context *ctx)
{
	struct ams_shared_context __sparse_cache *shared_c;

	shared_c = ams_acquire(ctx);
	shared_c->last_used_msg_id = AMS_INVALID_MSG_TYPE;
	ams_release(shared_c);

	return 0;
}

int ams_init(void)
{
	struct ams_shared_context *ams_shared_ctx;
	struct async_message_service **ams = arch_ams_get();
	struct sof *sof;
	int ret = 0;

	*ams = rzalloc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
		       sizeof(**ams));
	if (!*ams)
		return -ENOMEM;

	(*ams)->ams_context = &ctx[cpu_get_id()];
	memset((*ams)->ams_context, 0, sizeof(*(*ams)->ams_context));

	if (cpu_get_id() == PLATFORM_PRIMARY_CORE_ID) {
		sof = sof_get();
		sof->ams_shared_ctx = coherent_init(struct ams_shared_context, c);
		if (!sof->ams_shared_ctx)
			goto err;
		coherent_shared(sof->ams_shared_ctx, c);
	}

	ams_shared_ctx = ams_ctx_get();
	(*ams)->ams_context->shared = ams_shared_ctx;

	ams_create_shared_context((*ams)->ams_context->shared);

#if CONFIG_SMP
	ret = ams_task_init();
#endif /* CONFIG_SMP */

	return ret;

err:
	rfree(*ams);

	return -ENOMEM;
}
