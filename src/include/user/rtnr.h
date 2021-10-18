/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Realtek Semiconductor Corp. All rights reserved.
 *
 * Author: Ming Jen Tai <mingjen_tai@realtek.com>
 */

#ifndef __USER_RTNR_H__
#define __USER_RTNR_H__

#include <stdint.h>

#define SOF_RTNR_MAX_SIZE 256

struct sof_rtnr_params {
	/* 1 to enable RTNR, 0 to disable it */
	int32_t enabled;
	uint32_t sample_rate;
	int32_t reserved;
} __attribute__((packed, aligned(4)));

struct sof_rtnr_config {
	uint32_t size;

	/* reserved */
	uint32_t reserved[4];

	struct sof_rtnr_params params;
} __attribute__((packed, aligned(4)));

#endif /* __USER_RTNR_H__ */

