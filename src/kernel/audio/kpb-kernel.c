// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

/*
 * The kernel part of the KPB: the kernel half of the kpb_request_draining()
 * syscall. The rest of the KPB is in src/audio/kpb.c.
 */

#include <rtos/symbol.h>
#include <rtos/userspace_data.h>
#include <sof/audio/component.h>
#include <sof/audio/kpb.h>
#include <zephyr/sys/atomic.h>
#include <errno.h>
#include <stdint.h>

/*
 * Kernel code writes it, the KPB instances read it in the LL threads, which
 * can be user threads.
 */
APP_SYSUSER_BSS atomic_t kpb_drain_request;

int z_impl_kpb_request_draining(uint32_t id, uint32_t drain_req)
{
	atomic_val_t old;
	atomic_val_t new;

	if (id >= KPB_MAX_NO_OF_CLIENTS || drain_req > KPB_MAX_DRAINING_REQ)
		return -EINVAL;

	do {
		old = atomic_get(&kpb_drain_request);
		new = KPB_DRAIN_REQ(KPB_DRAIN_REQ_SEQ(old) + 1, id, drain_req);
	} while (!atomic_cas(&kpb_drain_request, old, new));

	return 0;
}
EXPORT_SYMBOL(z_impl_kpb_request_draining);
