/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_MN_H__
#define __SOF_DRIVERS_MN_H__

#include <platform/drivers/mn.h>
#include <sof/sof.h>

#include <stdbool.h>
#include <stdint.h>

/**
 * \brief Initializes MN driver.
 */
void mn_init(struct sof *sof);

/**
 * \brief Finds and sets valid combination of MCLK source and divider to
 *	  achieve requested MCLK rate.
 *	  User should release clock when it is no longer needed to allow
 *	  driver to change MCLK M/N source when user count drops to 0.
 *	  M value of M/N is not supported for MCLK, only divider can be used.
 * \param[in] mclk_id id of main clock for which rate should be set.
 * \param[in] mclk_rate main clock frequency.
 * \return 0 on success otherwise a negative error code.
 */
int mn_set_mclk(uint16_t mclk_id, uint32_t mclk_rate);

/**
 * \brief set mclk according to blob setting
 * \param[in] mdivc mclk control setting.
 * \param[in] mdivr mclk divider setting.
 * \return 0 on success otherwise a negative error code.
 */
int mn_set_mclk_blob(uint32_t mdivc, uint32_t mdivr);

/**
 * \brief Release previously requested MCLK for given MCLK ID.
 * \param[in] mclk_id id of main clock.
 */
void mn_release_mclk(uint32_t mclk_id);

#if CONFIG_INTEL_MN
/**
 * \brief Finds and sets valid combination of BCLK source and M/N to
 *	  achieve requested BCLK rate.
 *	  User should release clock when it is no longer needed to allow
 *	  driver to change M/N source when user count drops to 0.
 * \param[in] dai_index DAI index (SSP port).
 * \param[in] bclk_rate Bit clock frequency.
 * \param[out] out_scr_div SCR divisor that should be set by caller to achieve
 *			   requested BCLK rate.
 * \param[out] out_need_ecs If set to true, the caller should configure ECS.
 * \return 0 on success otherwise a negative error code.
 * \see mn_release_bclk()
 */
int mn_set_bclk(uint32_t dai_index, uint32_t bclk_rate,
		uint32_t *out_scr_div, bool *out_need_ecs);

/**
 * \brief Release previously requested BCLK for given DAI.
 * \param[in] dai_index DAI index (SSP port).
 */
void mn_release_bclk(uint32_t dai_index);

/**
 * \brief Resets M & N values of M/N divider for given DAI index.
 * \param[in] dai_index DAI index (SSP port).
 */
void mn_reset_bclk_divider(uint32_t dai_index);
#endif

/**
 * \brief Retrieves M/N dividers structure.
 * \return Pointer to M/N dividers structure.
 */
static inline struct mn *mn_get(void)
{
	return sof_get()->mn;
}

#endif /* __SOF_DRIVERS_MN_H__ */
