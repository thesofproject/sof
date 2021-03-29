/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Waves Audio Ltd. All rights reserved.
 */
#ifndef MAXX_EFFECT_RPC_SERVER_H
#define MAXX_EFFECT_RPC_SERVER_H

#include <stdint.h>
#include "MaxxEffect/MaxxEffect.h"
#include "MaxxEffect/MaxxStatus.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Get required buffers size for communication.
 *
 * @param[in]  effect           initialized effect
 * @param[out] requestBytes     request buffer size
 * @param[out] responseBytes    response buffer size
 *
 * @return 0 if success, otherwise fail
 ******************************************************************************/
MaxxStatus_t    MaxxEffect_GetMessageMaxSize(
    MaxxEffect_t*   effect,
    uint32_t*       requestBytes,
    uint32_t*       responseBytes);

/*******************************************************************************
 * Process request buffer and provide with the response.
 *
 * @param[in]  effect           initialized effect
 * @param[in]  request          request buffer
 * @param[in]  requestBytes     request buffer size
 * @param[out] response         response buffer
 * @param[out] responseBytes    response buffer size
 *
 * @return 0 if success, otherwise fail
 ******************************************************************************/
MaxxStatus_t    MaxxEffect_Message(
    MaxxEffect_t*   effect,
    const void*     request,
    uint32_t        requestBytes,
    void*           response,
    uint32_t*       responseBytes);

#ifdef __cplusplus
}
#endif

#endif
