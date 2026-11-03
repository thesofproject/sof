// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2026 Intel Corporation.

#include <sof/audio/component.h>
#include <sof/audio/kpb.h>
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>

/**
 * \brief Userspace verification wrapper for kpb_notifier_schedule().
 *
 * Only scalar arguments are passed, their ranges are checked by the
 * implementation, so no memory access validation is required.
 *
 * \param[in] id Client ID, i.e. the KPB output pin.
 * \param[in] drain_req Requested draining length in milliseconds.
 * \return 0 on success, negative error code otherwise.
 */
static inline int z_vrfy_kpb_notifier_schedule(uint32_t id, uint32_t drain_req)
{
	return z_impl_kpb_notifier_schedule(id, drain_req);
}
#include <zephyr/syscalls/kpb_notifier_schedule_mrsh.c>
