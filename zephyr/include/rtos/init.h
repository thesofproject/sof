/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#ifndef __ZEPHYR_RTOS_INIT_H__
#define __ZEPHYR_RTOS_INIT_H__

#include <zephyr/init.h>

#define SOF_MODULE_INIT(name, init) \
static int zephyr_##name##_init(void) \
{ \
	init(); \
	return 0; \
} \
SYS_INIT(zephyr_##name##_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY)

#endif /* __ZEPHYR_RTOS_INIT_H__ */
