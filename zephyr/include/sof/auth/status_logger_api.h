/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 *         Pawel Dobrowolski <pawelx.dobrowolski@intel.com>
 */
#ifndef STATUS_LOGGER_API
#define STATUS_LOGGER_API

#include "stdint.h"
#include "status_logger_iface.h"

inline void sts_log_init(struct status_logger_ctx *ctx)
{
	ctx->cb.init(ctx);
}

inline void sts_log_cleanup(struct status_logger_ctx *ctx)
{
	ctx->cb.cleanup(ctx);
}

inline void sts_log_report_error(struct status_logger_ctx *ctx, int error_code)
{
	ctx->cb.report_error(ctx, error_code);
}

inline void sts_log_set_boot_state(struct status_logger_ctx *ctx, uint32_t state)
{
	ctx->cb.set_boot_state(ctx, state);
}

inline void sts_log_set_wait_state(struct status_logger_ctx *ctx, uint32_t state)
{
	ctx->cb.set_wait_state(ctx, state);
}

inline void sts_log_set_module(struct status_logger_ctx *ctx, uint32_t mod)
{
	ctx->cb.set_module(ctx, mod);
}

#endif /*STATUS_LOGGER_API*/
