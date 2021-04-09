/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intelligo Technology Inc. All rights reserved.
 *
 * Author: Fu-Yun TSUO <fy.tsuo@intelli-go.com>
 */

#ifndef __USER_IGO_NR_H__
#define __USER_IGO_NR_H__

#include <stdint.h>

struct sof_igo_nr_config {
	/* reserved */
	struct IGO_PARAMS igo_params;

	int16_t data[];
} __attribute__((packed));

#endif /* __USER_IGO_NR_H__ */
