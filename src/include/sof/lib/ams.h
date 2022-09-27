/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2023 Intel Corporation
 *
 * Author: Krzysztof Frydryk <krzysztofx.frydryk@intel.com>
 */

#ifndef __SOF_LIB_AMS_H__
#define __SOF_LIB_AMS_H__

#include <errno.h>
#include <rtos/task.h>
#include <ipc/topology.h>
#include <rtos/alloc.h>
#include <sof/coherent.h>
#include <sof/lib/uuid.h>

/* Reserved value "does not exist" or "unassigned" value for msg types */
#define AMS_INVALID_MSG_TYPE 0
/* Reserved value "does not exist" or "unassigned" value for slots */
#define AMS_INVALID_SLOT 0xFF
/* Wildcard for module_id and instance_id values */
#define AMS_ANY_ID 0xFFFF

/* max number of message UUIDs */
#define AMS_SERVICE_UUID_TABLE_SIZE 16
/* max number of async message routes */
#define AMS_ROUTING_TABLE_SIZE 16
/* Space allocated for async message content*/
#define AMS_MAX_MSG_SIZE 0x1000

/* Size of slots message, module id and instance id */
#define AMS_SLOT_SIZE(msg) (AMS_MESSAGE_SIZE(msg) + sizeof(uint16_t) * 2)
#define AMS_MESSAGE_SIZE(msg) (sizeof(*msg) - sizeof(char) + (sizeof(char) * (msg->message_length)))

/**
 * \brief IXC message payload
 *
 * ams_message_payload - contains the actual Async Msg payload
 */
struct ams_message_payload {
	/* Message IDs are assigned dynamically on new message entry creation
	 * For a new payload should be acquired by ams_get_message_type_id
	 */
	uint32_t message_type_id;
	/* Producers module ID */
	uint16_t producer_module_id;
	/* Producers instance ID */
	uint16_t producer_instance_id;
	/* Message length */
	uint32_t message_length;
	/* Message payload */
	uint8_t *message;
};

struct ams_slot {
	uint16_t module_id;
	uint16_t instance_id;
	union {
		struct ams_message_payload msg;
		uint8_t msg_raw[AMS_MAX_MSG_SIZE];
	} u;
	uint32_t __aligned(PLATFORM_DCACHE_ALIGN) pad[];
};

/**
 * \brief ams_msg_callback_fn
 *
 * Each subscriber provides this handler function for each message ID
 */
typedef void (*ams_msg_callback_fn)(const struct ams_message_payload *const ams_message_payload,
				    void *ctx);

/**
 * \brief Internal struct ams_consumer_entry
 *
 * Describes a single consumer's subscription to a single message.
 * Array of 'ams_consumer_entry' structs forms AsyncMessageService's routing
 * table which allows for message dispatch.
 */
struct ams_consumer_entry {
	/* Message ID that will be routed via this entry */
	uint32_t message_type_id;
	/* Callback provided by the subscribed consumer */
	ams_msg_callback_fn consumer_callback;
	/* Additional context for consumer_callback (optional) */
	void *ctx;
	/* Subscribed consumer's Module ID */
	uint16_t consumer_module_id;
	/* Subscribed consumer's Module Instance ID */
	uint8_t consumer_instance_id;
	/* Subscribed consumer's Module core id. Saved to speed up routing */
	uint8_t consumer_core_id;
};

struct ams_producer {
	/* Message ID that will be routed via this entry */
	uint32_t message_type_id;
	/* Subscribed producer's Module ID */
	uint16_t producer_module_id;
	/* Subscribed producer's Module Instance ID */
	uint8_t producer_instance_id;
};

struct uuid_idx {
	uint32_t message_type_id;
	uint8_t message_uuid[UUID_SIZE];
};

struct ams_shared_context {
	/* should be only used with ams_acquire/release function, not generic ones */
	struct coherent c;

	uint32_t last_used_msg_id;
	struct ams_consumer_entry rt_table[AMS_ROUTING_TABLE_SIZE];
	struct ams_producer producer_table[AMS_ROUTING_TABLE_SIZE];
	struct uuid_idx uuid_table[AMS_SERVICE_UUID_TABLE_SIZE];

	uint32_t slot_uses[CONFIG_CORE_COUNT];
	/* marks which core already processed slot */
	uint32_t slot_done[CONFIG_CORE_COUNT];

	struct ams_slot slots[CONFIG_CORE_COUNT];
};

struct ams_context {
	/* shared context must be always accessed with shared->c taken */
	struct ams_shared_context *shared;
};

struct ams_task {
	struct task ams_task;
	struct async_message_service *ams;
	uint32_t pending_slots;
};

struct async_message_service {
#if CONFIG_SMP
	struct ams_task ams_task;
#endif /* CONFIG_SMP */
	struct ams_context *ams_context;
};

#if CONFIG_AMS
int ams_init(void);

/**
 * \brief Get Message Type ID
 *
 * assigns and returns a message type ID for specified message UUID.
 * The value of message type ID is dynamically assigned and it will change between runs.
 *
 * \param[in] message_uuid UUID of message type
 * \param[in] message_type_id Unique message type ID assigned by AMS
 */
int ams_get_message_type_id(const uint8_t *message_uuid,
			    uint32_t *message_type_id);

/**
 * \brief Producer Register
 *
 * registers a producer of asynchronous messages of given message type.
 * When a module instance calls this function,
 * it informs the Asynchronous Messaging Service that it will be sending asynchronous messages.
 *
 * \param[in] message_type_id unique message type ID assigned during ams_get_message_type_id
 * \param[in] module_id Module ID of module calling function
 * \param[in] instance_id Instance ID of module calling function
 */
int ams_register_producer(uint32_t message_type_id,
			  uint16_t module_id,
			  uint16_t instance_id);

/**
 * \brief Producer Unregister
 *
 * unregisters a producer of asynchronous messages of given type.
 * When a module instance calls this function,
 * it informs the Asynchronous Messaging Service
 * that it will not be sending asynchronous messages anymore.
 *
 * \param[in] message_type_id unique message type ID assigned during ams_get_message_type_id
 * \param[in] module_id Module ID of module calling function
 * \param[in] instance_id Instance ID of module calling function
 */
int ams_unregister_producer(uint32_t message_type_id,
			    uint16_t module_id,
			    uint16_t instance_id);

/**
 * \brief Register Consumer
 *
 * Registers a module instance as a consumer of specified message type.
 * When specified message type is sent,
 * a callback is called that was provided during registration process.
 *
 * The consumer callback is triggered when ams_send function was used to send a message
 * and/or when ams_send_mi function with consumer's module ID and instance ID was used
 * to send a message.
 *
 * \param[in] message_type_id unique message type ID assigned during ams_get_message_type_id
 * \param[in] module_id Module ID of module calling function
 * \param[in] instance_id Instance ID of module calling function
 * \param[in] function callback that should be called when message is received
 * \param[in] ctx Optional context that is passed to callback
 */
int ams_register_consumer(uint32_t message_type_id,
			  uint16_t module_id,
			  uint16_t instance_id,
			  ams_msg_callback_fn function,
			  void *ctx);

/**
 * \brief Unegister Consumer
 *
 * Unregisters a consumer of specified message type
 *
 * \param[in] message_type_id unique message type ID assigned during ams_get_message_type_id
 * \param[in] module_id Module ID of module calling function
 * \param[in] instance_id Instance ID of module calling function
 * \param[in] function callback that should be called when message is received
 */
int ams_unregister_consumer(uint32_t message_type_id,
			    uint16_t module_id,
			    uint16_t instance_id,
			    ams_msg_callback_fn function);

/**
 * \brief Message Send
 *
 * Sends asynchronous message to all registered consumers by registered producer.
 * The consumers registered on the same core may be called in context of a message producer
 *
 * \param[in] payload Message payload
 */
int ams_send(const struct ams_message_payload *payload);

/**
 * \brief Message Send to Module Instance
 *
 * Sends asynchronous message to specified module instance.
 * The consumer registered on the same core may be called in context of a message producer
 *
 * \param[in] payload Message payload
 * \param[in] module_id Module ID of consumer that messages is sent to
 * \param[in] instance_id Instance ID of consumer that messages is sent to
 */
int ams_send_mi(const struct ams_message_payload *payload,
		uint16_t module_id, uint16_t instance_id);

static inline struct ams_shared_context *ams_ctx_get(void)
{
	return sof_get()->ams_shared_ctx;
}
#else
static inline int ams_init(void) { return 0; }
static inline int ams_get_message_type_id(const uint8_t *message_uuid,
					  uint32_t *message_type_id) { return 0; }

static inline int ams_register_producer(uint32_t message_type_id,
					uint16_t module_id,
					uint16_t instance_id) { return 0; }

static inline int ams_unregister_producer(uint32_t message_type_id,
					  uint16_t module_id,
					  uint16_t instance_id) { return 0; }

static inline int ams_register_consumer(uint32_t message_type_id,
					uint16_t module_id,
					uint16_t instance_id,
					ams_msg_callback_fn function,
					void *ctx) { return 0; }

static inline int ams_unregister_consumer(uint32_t message_type_id,
					  uint16_t module_id,
					  uint16_t instance_id,
					  ams_msg_callback_fn function) { return 0; }

static inline int ams_send(const struct ams_message_payload *payload) { return 0; }

static inline int ams_send_mi(const struct ams_message_payload *payload, uint16_t module_id,
			      uint16_t instance_id) { return 0; }

static inline struct ams_shared_context *ams_ctx_get(void)
{
	return NULL;
}

#endif /* CONFIG_AMS */

#if CONFIG_SMP && CONFIG_AMS
int process_incoming_message(uint32_t slot);
#else
static inline int process_incoming_message(uint32_t slot) { return 0; }
#endif /* CONFIG_SMP && CONFIG_AMS */

struct async_message_service **arch_ams_get(void);

#endif /* __SOF_LIB_AMS_H__ */
