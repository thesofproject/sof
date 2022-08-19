/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_STRING_H__
#define __SOF_STRING_H__

#include <arch/string.h>
#include <stddef.h>

/* C memcpy for arch that don't have arch_memcpy() */
void cmemcpy(void *dest, void *src, size_t size);
int memcmp(const void *p, const void *q, size_t count);
int rstrlen(const char *s);
int rstrcmp(const char *s1, const char *s2);

#if defined(arch_memcpy)
#define rmemcpy(dest, src, size) \
	arch_memcpy(dest, src, size)
#else
#define rmemcpy(dest, src, size) \
	cmemcpy(dest, src, size)
#endif

#endif /* __SOF_STRING_H__ */
