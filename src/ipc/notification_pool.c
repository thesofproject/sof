// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Adrian Warecki <adrian.warecki@intel.com>
//

#include <stdint.h>
#include <sof/common.h>
#include <sof/list.h>
#include <sof/ipc/notification_pool.h>

#include <rtos/symbol.h>

#define NOTIFICATION_POOL_MAX_PAYLOAD_SIZE	40	/* IPC4 Resource Event needs 10dw */
#define NOTIFICATION_POOL_MAX_DEPTH		8	/* Maximum number of notifications
							 * in the pool
							 */

LOG_MODULE_REGISTER(notification_pool, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(notification_pool);

DECLARE_TR_CTX(notif_tr, SOF_UUID(notification_pool_uuid), LOG_LEVEL_INFO);

struct ipc_notif_pool_item {
	struct ipc_msg msg;
	uint32_t payload[SOF_DIV_ROUND_UP(NOTIFICATION_POOL_MAX_PAYLOAD_SIZE, sizeof(uint32_t))];
};

static struct list_item pool_free_list = LIST_INIT(pool_free_list);
static struct k_spinlock pool_free_list_lock;
static int pool_depth;

static void ipc_notif_free(struct ipc_msg *msg)
{
	struct ipc_notif_pool_item *item = container_of(msg, struct ipc_notif_pool_item, msg);
	k_spinlock_key_t key;

	key = k_spin_lock(&pool_free_list_lock);
	list_item_append(&item->msg.list, &pool_free_list);
	k_spin_unlock(&pool_free_list_lock, key);
}

static struct ipc_msg *ipc_notif_new(size_t size)
{
	struct ipc_notif_pool_item *item;

	item = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*item));
	if (!item) {
		tr_err(&notif_tr, "Unable to allocate memory for notification message");
		return NULL;
	}

	list_init(&item->msg.list);
	item->msg.tx_data = item->payload;
	item->msg.tx_size = size;
	item->msg.callback = ipc_notif_free;
	return &item->msg;
}

struct ipc_msg *ipc_notification_pool_get(size_t size)
{
	struct ipc_notif_pool_item *item;
	k_spinlock_key_t key;
	struct ipc_msg *new_msg;

	if (size > NOTIFICATION_POOL_MAX_PAYLOAD_SIZE) {
		tr_err(&notif_tr, "Requested size %zu exceeds maximum payload size %u",
		       size, NOTIFICATION_POOL_MAX_PAYLOAD_SIZE);
		return NULL;
	}

	/* check if we have a free message */
	key = k_spin_lock(&pool_free_list_lock);
	if (list_is_empty(&pool_free_list)) {
		/* allocate a new message */
		if (pool_depth >= NOTIFICATION_POOL_MAX_DEPTH) {
			k_spin_unlock(&pool_free_list_lock, key);
			tr_err(&notif_tr, "Pool depth exceeded");
			return NULL;
		}
		++pool_depth;
		k_spin_unlock(&pool_free_list_lock, key);

		new_msg = ipc_notif_new(size);
		if (!new_msg) {
			key = k_spin_lock(&pool_free_list_lock);
			--pool_depth;
			k_spin_unlock(&pool_free_list_lock, key);
		}
		return new_msg;
	}

	/* take the first free message */
	item = list_first_item(&pool_free_list, struct ipc_notif_pool_item, msg.list);
	list_item_del(&item->msg.list);
	k_spin_unlock(&pool_free_list_lock, key);

	item->msg.tx_size = size;
	return &item->msg;
}
EXPORT_SYMBOL(ipc_notification_pool_get);
