/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Michal Jerzy Wierzbicki <michalx.wierzbicki@linux.intel.com>
 */

#include <test_simple_macro.h>
#include <sof/trace/preproc.h>
#include <rtos/sof.h>
#include <rtos/alloc.h>

/* CMOCKA SETUP */
#define setup_alloc(ptr, type, size, offset) do {\
	if (ptr)\
		free(ptr);\
	ptr = malloc(sizeof(type) * (size) + offset);\
	if (ptr == NULL)\
		return -1;\
} while (0)

#define setup_part(result, setup_func) do {\
	result |= setup_func;\
	if (result)\
		return result;\
} while (0)

