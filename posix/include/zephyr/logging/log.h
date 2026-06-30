/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 *
 * Stub for <zephyr/logging/log.h> used by testbench / posix builds.
 */

#ifndef __POSIX_ZEPHYR_LOGGING_LOG_H__
#define __POSIX_ZEPHYR_LOGGING_LOG_H__

#include <stdio.h>

#ifndef LOG_ERR
#define LOG_ERR(fmt, ...)	fprintf(stderr, "ERR: " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef LOG_WRN
#define LOG_WRN(fmt, ...)	fprintf(stderr, "WRN: " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef LOG_INF
#define LOG_INF(fmt, ...)	printf("INF: " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef LOG_DBG
#define LOG_DBG(fmt, ...)	do { } while (0)
#endif

#ifndef LOG_MODULE_REGISTER
#define LOG_MODULE_REGISTER(ctx, level)
#endif
#ifndef LOG_MODULE_DECLARE
#define LOG_MODULE_DECLARE(ctx, level)
#endif

#endif /* __POSIX_ZEPHYR_LOGGING_LOG_H__ */
