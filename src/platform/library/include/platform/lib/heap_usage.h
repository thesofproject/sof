/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 */

#ifndef __SOF_LIB_HEAP_USAGE_H__
#define __SOF_LIB_HEAP_USAGE_H__

#include <stdbool.h>
#include <stddef.h>

struct platform_library_heap_usage {
	bool enable;
	size_t rmalloc_size;
	size_t rzalloc_size;
	size_t rballoc_align_size;
	size_t rbrealloc_align_size;
};

#endif /* __SOF_LIB_HEAP_USAGE_H__ */

