/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_MATH_NUMBERS_H__
#define __SOF_MATH_NUMBERS_H__

#include <stdint.h>

 /* Zephyr defines this - remove local copy once Zephyr integration complete */
#ifdef MIN
#undef MIN
#endif

#define MIN(a, b) ({		\
	typeof(a) __a = (a);	\
	typeof(b) __b = (b);	\
	__a > __b ? __b : __a;	\
})

/* Zephyr defines this - remove local copy once Zephyr integration complete */
#ifdef MAX
#undef MAX
#endif

#define MAX(a, b) ({		\
	typeof(a) __a = (a);	\
	typeof(b) __b = (b);	\
	__a < __b ? __b : __a;	\
})
//#define ABS(a) ({		\
//	typeof(a) __a = (a);	\
//	__a < 0 ? -__a : __a;	\
//})
#define ABS(a)((a < 0)?(-a):(a))
//#define SGN(a) ({		\
//	typeof(a) __a = (a);	\
//	__a < 0 ? -1 :		\
//	__a > 0 ? 1 : 0;	\
//})
#define SGN(x) (((x) < 0)  ?  -1   : ((x) > 0) ? 1 : 0)
#endif /* __SOF_MATH_NUMBERS_H__ */
