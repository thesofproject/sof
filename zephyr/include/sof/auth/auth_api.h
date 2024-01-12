/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 *         Pawel Dobrowolski <pawelx.dobrowolski@intel.com>
 */
#ifndef AUTH_API_H
#define AUTH_API_H

#include "auth_api_iface.h"

#define AUTH_API_CALLBACKS_ADDR (0x162000 + 0x140)

/* The same as auth_api->init. */
inline int auth_api_init(struct auth_api_ctx *ctx, void* scratch_buff,
			size_t scratch_buff_size, enum auth_image_type image_type)
{
	struct auth_api_ctx **auth_api_callbacks = (struct auth_api_ctx **)(AUTH_API_CALLBACKS_ADDR);

	ctx->version_api = (*auth_api_callbacks)->version_api;
	ctx->auth_api = (*auth_api_callbacks)->auth_api;
	return ctx->auth_api->init(ctx, scratch_buff, scratch_buff_size, image_type);
}

/* The same as auth_api->cleanup. */
inline void auth_api_cleanup(struct auth_api_ctx *ctx)
{
	ctx->auth_api->cleanup(ctx);
}

/* The same as auth_api->init_auth_proc. */
inline int auth_api_init_auth_proc(struct auth_api_ctx *ctx, const void* chunk, size_t chunk_size,
			enum auth_phase phase)
{
	return ctx->auth_api->init_auth_proc(ctx, chunk, chunk_size, phase);
}

/* The same as auth_api->busy. */
inline bool auth_api_busy(struct auth_api_ctx *ctx)
{
	return ctx->auth_api->busy(ctx);
}

/* The same as auth_api->result. */
inline enum auth_result auth_api_result(struct auth_api_ctx *ctx)
{
	return ctx->auth_api->result(ctx);
}

/* The same as auth_api->register_status_logger. */
inline int auth_api_register_status_logger(struct auth_api_ctx *ctx,
			struct status_logger_ctx* status_logger)
{
	return ctx->auth_api->register_status_logger(ctx, status_logger);
}

/* The same as AuthApi::UnregisterStatusLogger. */
inline void auth_api_unregister_status_logger(struct auth_api_ctx *ctx)
{
	ctx->auth_api->unregister_status_logger(ctx);
}

/* The same as auth_api->version. */
inline struct auth_api_version_num auth_api_version()
{
  struct auth_api_ctx **auth_api_callbacks = (struct auth_api_ctx **)(AUTH_API_CALLBACKS_ADDR);
	return (*auth_api_callbacks)->version_api->version();
}

#endif /* AUTH_API_H */

