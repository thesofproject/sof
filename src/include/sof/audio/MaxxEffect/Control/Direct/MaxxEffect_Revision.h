/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Waves Audio Ltd. All rights reserved.
 */
#ifndef MAXX_EFFECT_REVISION_H
#define MAXX_EFFECT_REVISION_H

#include <stdint.h>
#include "MaxxEffect/MaxxEffect.h"
#include "MaxxEffect/MaxxStatus.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Get null-terminated revision string.
 *
 * @param[in]  effect   Initialized effect.
 * @param[out] revision Revision string.
 * @param[out] bytes    Revision string size. Optional, use NULL if not needed.
 *
 * @return 0 if success, otherwise fail
 ******************************************************************************/
MaxxStatus_t    MaxxEffect_Revision_Get(
    MaxxEffect_t*       effect,
    const char**        revision,
    uint32_t*           bytes);

#ifdef __cplusplus
}
#endif

#endif
