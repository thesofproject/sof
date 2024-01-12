/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 *         Pawel Dobrowolski <pawelx.dobrowolski@intel.com>
 */
#ifndef __AUTH_API_IFACE_H__
#define __AUTH_API_IFACE_H__

#include "status_logger_iface.h"
#include <stdint.h>
#include <stddef.h>

#define AUTH_API_VERSION_MAJOR (2)
#define AUTH_API_VERSION_MINOR (0)
#define AUTH_API_VERSION_PATCH (0)

#define AUTH_SCRATCH_BUFF_SZ (0xA000) // 40kB

/*
 * Return codes supported by authentication engine:
 * ADSP_AUTH_IMAGE_UNTRUSTED               = 9040,
 * ADSP_AUTH_CANNOT_ALLOCATE_SCRATCH_BUFF  = 9041,
 * ADSP_AUTH_INVALID_AUTH_API_CTX_PTR      = 9042,
 * ADSP_AUTH_SVN_VERIFICATION_FAIL         = 9043,
 * ADSP_AUTH_IFWI_PARTITION_FAIL           = 9044,
 * ADSP_AUTH_VERIFY_IMAGE_TYPE_FAIL        = 9045,
 * ADSP_AUTH_UNSUPPORTED_VERSION           = 9046,
 * ADSP_AUTH_INCOMPATIBLE_MANIFEST_VERSION = 9047,
 */

struct auth_api_version_num {
	uint8_t patch;
	uint8_t minor;
	uint8_t major;
	uint8_t rsvd;
} __attribute__((packed, aligned(4)));

enum auth_phase {
	AUTH_PHASE_FIRST = 0,
	AUTH_PHASE_MID = 1,
	AUTH_PHASE_LAST = 2
};

enum auth_result {
	AUTH_NOT_COMPLETED = 0,
	AUTH_IMAGE_TRUSTED = 1,
	AUTH_IMAGE_UNTRUSTED = 2
};

enum auth_image_type {
	IMG_TYPE_ROM_EXT = 0,
	IMG_TYPE_MAIN_FW = 1,
	IMG_TYPE_LIB = 2
};

struct auth_api_ctx;

struct auth_api_version {
	/* Interface to return authentication API version.
	 * Return value: version number represented by AuthApiVersionNum structue.
	 */
	struct auth_api_version_num (*version)();
};

struct auth_api {
	/* Interface to initialize authentication API and context.
	 * Parameters:
	 * ctx - pointer to the context instance of type AuthApiCtx.
	 * scratch_buff - pointer to scratch buffer.
	 *    Scratch buffer must be located in L2 Local Memory (SHA Engine limitation).
	 *    Caller is responsible to power up necessary L2 Local Memory banks.
	 *    Address alignment must correspond to SHA384_IO_BUF_ALIGNMENT.
	 * scratch_buff_size – size must be the same as AUTH_SCRATCH_BUFF_SZ.
	 * Return value:
	 *    ADSP_SUCCESS - successfull initialization.
	 */
	int (*init)(struct auth_api_ctx *ctx, void *scratch_buff, size_t scratch_buff_size,
			enum auth_image_type image_type);

	/* Interface to cleanup authentication API.
	 * Parameters:
	 * ctx - pointer to the context instance of type AuthApiCtx.
	 */
	void (*cleanup)(struct auth_api_ctx *ctx);

	/* Interface for initiating signed FW image (async) authentication process.
	 * Parameters:
	 * ctx - pointer to the context instance of type AuthApiCtx.
	 * chunk - pointer to the chunk of signed FW image.
	 * chunk_size - chunk size in bytes.
	 * phase - authentication phase.
	 *    Must corresponds to one of the AuthPhase values.
	 *    In case of one time FW authentication, where signed FW image size must be
	 *    less or equal to scratch_buff_size, the caller must pass AUTH_PHASE_LAST.
	 * Return value: ADSP_SUCCESS when authentication process has been initiated
	 *    successfully, or one of ADSP_FLV_* error codes in case of failure.
	 */
	int (*init_auth_proc)(struct auth_api_ctx *ctx, const void *chunk, size_t chunk_size,
			enum auth_phase phase);

	/* Interface to return if authentication process is busy.
	 * Parameters:
	 * ctx - pointer to the context instance of type AuthApiCtx.
	 * This function can be used for authentication process synchronization.
	 * Return value: true if authentication process is busy.
	 */
	bool (*busy)(struct auth_api_ctx *ctx);

	/* Interface to return authentication result
	 * Parameters:
	 * ctx - pointer to the context instance of type AuthApiCtx.
	 * Return value:
	 *    AUTH_NOT_COMPLETED - authentication is not completed,
	 *    AUTH_IMAGE_TRUSTED - authentication completed and signed FW image is
	 *        trusted,
	 *    AUTH_IMAGE_UNTRUSTED - authentication completed, but signed FW image is
	 *        untrusted.
	 */
	enum auth_result (*result)(struct auth_api_ctx *ctx);

	/* Interface to register status/error code logger.
	 * Parameters:
	 * ctx - pointer to the context instance of type AuthApiCtx.
	 * sts_logger - pointer to status logger.
	 * Return value: ADSP_SUCCESS when logger has been registered successfully.
	 */
	int (*register_status_logger)(struct auth_api_ctx *ctx,
		struct status_logger_ctx *status_logger);

	/* Interface to unregister status/error code logger.
	 * Parameters:
	 * ctx - pointer to the context instance of type AuthApiCtx.
	 */
	void (*unregister_status_logger)(struct auth_api_ctx *ctx);
};

struct auth_api_ctx {
	struct auth_api_version *version_api;
	void *scratch_buff;
	size_t scratch_buff_size;
	enum auth_result result;
	struct auth_api *auth_api;
	enum auth_image_type image_type;
	struct status_logger_ctx* status_logger;
};

#endif /* __SOF_LIB_MANAGER_H__ */
