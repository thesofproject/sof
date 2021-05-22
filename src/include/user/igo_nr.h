/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intelligo Technology Inc. All rights reserved.
 *
 * Author: Fu-Yun TSUO <fy.tsuo@intelli-go.com>
 */

#ifndef __USER_IGO_NR_H__
#define __USER_IGO_NR_H__

#include <stdint.h>

struct IGO_PARAMS {
	uint32_t igo_params_ver;
	uint32_t dump_data;
	uint32_t nr_bypass;
	uint32_t nr_mode1_en;
	uint32_t nr_mode3_en;
	uint32_t nr_ul_enable;
	uint32_t agc_gain;
	uint32_t nr_voice_str;
	uint32_t nr_level;
	uint32_t nr_mode1_floor;
	uint32_t nr_mode1_od;
	uint32_t nr_mode1_pp_param7;
	uint32_t nr_mode1_pp_param8;
	uint32_t nr_mode1_pp_param10;
	uint32_t nr_mode3_floor;
	uint32_t nr_mode1_pp_param53;
} __attribute__((packed));

struct sof_igo_nr_config {
	/* reserved */
	struct IGO_PARAMS igo_params;
	uint32_t active_channel_idx;
	int16_t data[];
} __attribute__((packed));

#endif /* __USER_IGO_NR_H__ */
