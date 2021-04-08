/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Waves Audio Ltd. All rights reserved.
 */
#ifndef MAXX_EFFECT_RESET_H
#define MAXX_EFFECT_RESET_H

#include "MaxxEffect/MaxxEffect.h"
#include "MaxxEffect/MaxxStatus.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Reset internal [states](@ref MaxxEffect_InternalData_States), and [meters]
 * (@ref MaxxEffect_InternalData_Meters). Configuration remains the same.
 * Should be called only if [MaxxEffect_GetActive](@ref MaxxEffect_GetActive)
 * is 0 to avoid audio artifacts.
 *
 * @param[in] effect initialized effect
 *
 * @return 0 on success, otherwise fail
 ******************************************************************************/
MaxxStatus_t    MaxxEffect_Reset(
    MaxxEffect_t*           effect);

#ifdef __cplusplus
};
#endif

#endif
