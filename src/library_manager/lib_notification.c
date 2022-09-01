// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
//         Pawel Dobrowolski <pawelx.dobrowolski@intel.com>
//

/*
 * 3-rd party modules do not have access to component device runtime data.
 * System service API used by these modules require notification handling
 * based on base FW message management. Therefore this code expose notification
 * calls aligned with cAVS/ACE System service API requirements.
 */
#include <sof/sof.h>
#include <sof/list.h>
#include <sof/ipc/msg.h>
#include <sof/lib/memory.h>
#include <sof/lib_manager.h>

LOG_MODULE_DECLARE(lib_manager, LOG_LEVEL_INFO);

struct ipc_msg *lib_notif_msg_init(uint32_t header, uint32_t size)
{
	struct ipc_msg *msg = NULL;
	struct ipc_lib_msg *msg_pool_elem;
	struct ext_library *ext_lib = ext_lib_get();
	struct ipc_lib_msg *lib_notif = ext_lib->lib_notif_pool;

	/* If list not empty search for free notification handle */
	if (lib_notif && !list_is_empty(&lib_notif->list)) {
		struct list_item *list_elem;
		/* Search for not used message handle */
		list_for_item(list_elem, &lib_notif->list) {
			msg_pool_elem = container_of(list_elem, struct ipc_lib_msg, list);
			if (msg_pool_elem->msg && list_is_empty(&msg_pool_elem->msg->list)) {
				msg = msg_pool_elem->msg;
				break;
			}
		}
	}

	if (!msg) {
		k_spinlock_key_t key;
		/* No free element or list empty, create new handle  */
		if (ext_lib->lib_notif_count > LIB_MANAGER_LIB_NOTIX_MAX_COUNT) {
			tr_dbg(&lib_manager_tr, "lib_nofig_msg_init() LIB_MANAGER_LIB_NOTIX_MAX_COUNT < %d",
			       ext_lib->lib_notif_count);
			return NULL;
		}

		msg_pool_elem = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0,
					SOF_MEM_CAPS_RAM, sizeof(*msg_pool_elem));
		if (!msg_pool_elem)
			return NULL;
		msg = ipc_msg_init(header, SRAM_OUTBOX_SIZE);
		if (!msg) {
			rfree(msg_pool_elem);
			return NULL;
		}
		msg_pool_elem->msg = msg;
		ext_lib->lib_notif_count++;
		list_init(&msg_pool_elem->list);
		/* Check if there is no rece between modules on separate cores */
		if (ext_lib->lib_notif_count > 1) {
			key = k_spin_lock(&ext_lib->lock);
			list_item_append(&msg_pool_elem->list, &lib_notif->list);
			k_spin_unlock(&ext_lib->lock, key);
		} else {
			ext_lib->lib_notif_pool = msg_pool_elem;
		}
	}
	/* Update header and size, since message handle can be reused */
	msg->header = header;
	msg->tx_size = size;
	return msg;
}

void lib_notif_msg_send(struct ipc_msg *msg)
{
	ipc_msg_send(msg, msg->tx_data, false);
	lib_notif_msg_clean(true);
}

void lib_notif_msg_clean(bool leave_one_handle)
{
	struct list_item *list_elem, *list_tmp;
	struct ipc_lib_msg *msg_pool_elem;
	struct ext_library *ext_lib = ext_lib_get();
	struct ipc_lib_msg *lib_notif = ext_lib->lib_notif_pool;

	/* Free all unused elements except the last one */
	list_for_item_safe(list_elem, list_tmp, &lib_notif->list) {
		msg_pool_elem = container_of(list_elem, struct ipc_lib_msg, list);
		assert(msg_pool_elem->msg);

		if (list_is_empty(&msg_pool_elem->msg->list)) {
			k_spinlock_key_t key;

			key = k_spin_lock(&ext_lib->lock);
			list_item_del(&msg_pool_elem->list);
			k_spin_unlock(&ext_lib->lock, key);
			ipc_msg_free(msg_pool_elem->msg);
			rfree(msg_pool_elem);
			ext_lib->lib_notif_count--;
		}
	}

	/* Remove last handle - this should happen when there is no external libraries */
	if (!leave_one_handle && (list_is_empty(&lib_notif->list))) {
		k_spinlock_key_t key;

		ipc_msg_free(lib_notif->msg);
		key = k_spin_lock(&ext_lib->lock);
		list_item_del(&lib_notif->list);
		k_spin_unlock(&ext_lib->lock, key);
		rfree(lib_notif);
		ext_lib->lib_notif_pool = NULL;
		ext_lib->lib_notif_count--;
	}
}
