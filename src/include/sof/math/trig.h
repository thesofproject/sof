/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_MATH_TRIG_H__
#define __SOF_MATH_TRIG_H__

#include <stdint.h>

#ifndef UNIT_CORDIC_TEST
#define CONFIG_CORDIC_TRIGONOMETRY_FIXED
#endif


#define PI_DIV2_Q4_28 421657428
#define PI_Q4_28      843314857
#define PI_MUL2_Q4_28     1686629713
#define CORDIC_31B_TABLE_SIZE		31
#define CORDIC_SIN_COS_15B_TABLE_SIZE	15

typedef enum {
	EN_32B_CORDIC_SINE,
	EN_32B_CORDIC_COSINE,
	EN_16B_CORDIC_SINE,
	EN_16B_CORDIC_COSINE,
} cordic_cfg;

void cordic_sin_cos(int32_t th_rad_fxp, cordic_cfg type, int32_t *sign, int32_t *b_yn, int32_t *xn,
		    int32_t *th_cdc_fxp);
/* Input is Q4.28, output is Q1.31 */
int32_t sin_fixed_32b(int32_t th_rad_fxp);
int32_t cos_fixed_32b(int32_t th_rad_fxp);
/* Input is Q4.28, output is Q1.15 */
int16_t sin_fixed_16b(int32_t th_rad_fxp);
int16_t cos_fixed_16b(int32_t th_rad_fxp);

#endif /* __SOF_MATH_TRIG_H__ */
