/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_MN_H__
#define __SOF_DRIVERS_MN_H__

#include <stdbool.h>
#include <stdint.h>

/** \brief Offset of MCLK Divider Control Register. */
#define MN_MDIVCTRL 0x0

/** \brief Enables the output of MCLK Divider. */
#define MN_MDIVCTRL_M_DIV_ENABLE BIT(0)

/** \brief Offset of MCLK Divider x Ratio Register. */
#define MN_MDIVR(x) (0x80 + (x) * 0x4)

/** \brief Offset of BCLK x M/N Divider M Value Register. */
#define MN_MDIV_M_VAL(x) (0x100 + (x) * 0x8 + 0x0)

/** \brief Offset of BCLK x M/N Divider N Value Register. */
#define MN_MDIV_N_VAL(x) (0x100 + (x) * 0x8 + 0x4)

/**
 * \brief Finds and sets valid combination of MCLK source and divider to
 *	  achieve requested MCLK rate.
 *	  M value of M/N is not supported for MCLK, only divider can be used.
 * \param[in] mclk_id id of master clock for which rate should be set.
 * \param[in] mclk_rate master clock frequency.
 * \return 0 on success otherwise a negative error code.
 */
int mn_set_mclk(uint16_t mclk_id, uint32_t mclk_rate);

/**
 * \brief Finds and sets valid combination of BCLK source and M/N to
 *	  achieve requested BCLK rate.
 * \param[in] dai_index DAI index (SSP port).
 * \param[in] bclk_rate Bit clock frequency.
 * \param[out] out_scr_div SCR divisor that should be set by caller to achieve
 *			   requested BCLK rate.
 * \param[out] out_need_ecs If set to true, the caller should configure ECS.
 * \return 0 on success otherwise a negative error code.
 */
int mn_set_bclk(uint32_t dai_index, uint32_t bclk_rate,
		uint32_t *out_scr_div, bool *out_need_ecs);

/**
 * \brief Resets M & N values of M/N divider for given DAI index.
 * \param[in] dai_index DAI index (SSP port).
 */
void mn_reset_bclk_divider(uint32_t dai_index);

#endif /* __SOF_DRIVERS_MN_H__ */
