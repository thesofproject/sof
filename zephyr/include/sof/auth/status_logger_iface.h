/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 *         Pawel Dobrowolski <pawelx.dobrowolski@intel.com>
 */
#ifndef STATUS_LOGGER_IFACE
#define STATUS_LOGGER_IFACE

#include "stdint.h"


struct status_logger_ctx;

/* Status Logger Interface callbacks. */
struct status_logger_iface {
	/* Interface to initialize Status Logger context.
	 * Parameters:
	 * @param status_logger_ctx *ctx - pointer to the context instance.
	 * Return value:
	 *    0 - SUCCESS - successfull initialization.
	 */
	int (*init)(struct status_logger_ctx *ctx);

	/* Interface to cleanup Status Logger context.
	 * Parameters:
	 * @param status_logger_ctx *ctx - pointer to the context instance.
	 */
	void (*cleanup)(struct status_logger_ctx *ctx);

	/**
	 * Reports critical rom error. Halts execution.
	 * Parameters:
	 * @param status_logger_ctx *ctx - pointer to the context instance.
	 * @param error_code - error code to report
	 */
	void (*report_error)(struct status_logger_ctx *ctx, int error_code);

	/**
	 * Reports boot status.
	 * Parameters:
	 * @param status_logger_ctx *ctx - pointer to the context instance.
	 * @param uint32_t state - boot state code to report.
	 */
	void (*set_boot_state)(struct status_logger_ctx *ctx, uint32_t state);

	/**
	 * Reports that caller is waiting on some external event or action.
	 * Parameters:
	 * @param status_logger_ctx *ctx - pointer to the context instance.
	 * @param uint32_t state - reason of waiting.
	 */
	void (*set_wait_state)(struct status_logger_ctx *ctx, uint32_t state);

	/**
	 * Set module type in FSR.
	 * Parameters:
	 * @param status_logger_ctx *ctx - pointer to the context instance.
	 * @param mod module id
	 */

	void (*set_module)(struct status_logger_ctx *ctx, uint32_t mod);
} __attribute__((packed, aligned(4)));

/* Design note: Compiler was not able to generate proper call assembly code using standard C++
 * inheritance in Auth API implementation. That's why we do explicit callbacks assignment,
 * following C-like OOP.
 */
struct status_logger_ctx {
	struct status_logger_iface cb;
};

#endif /*STATUS_LOGGER_IFACE*/
