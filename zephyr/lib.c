// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2022 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <rtos/string.h>

#include <stddef.h>
#include <stdint.h>

void *__vec_memcpy(void *dst, const void *src, size_t len)
{
	return memcpy(dst, src, len);
}

void *__vec_memset(void *dest, int data, size_t src_size)
{
	return memset(dest, data, src_size);
}
