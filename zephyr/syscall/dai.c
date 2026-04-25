// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <sof/lib/dai.h>
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>

static inline const struct device *z_vrfy_dai_get_device(enum sof_ipc_dai_type type,
							 uint32_t index)
{
	return z_impl_dai_get_device(type, index);
}
#include <zephyr/syscalls/dai_get_device_mrsh.c>
