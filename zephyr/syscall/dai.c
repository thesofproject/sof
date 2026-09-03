// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <sof/lib/dai.h>
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>

static inline const struct device *z_vrfy_dai_get_device(enum sof_ipc_dai_type type,
							 uint32_t index)
{
	const struct device *dev = z_impl_dai_get_device(type, index);

	if (dev && !k_object_is_valid(dev, K_OBJ_DRIVER_DAI))
		return NULL;

	return dev;
}
#include <zephyr/syscalls/dai_get_device_mrsh.c>
