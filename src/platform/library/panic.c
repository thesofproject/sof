// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/debug/panic.h>
#include <stdlib.h>

void __panic(uint32_t p, char *filename, uint32_t linenum)
{
	abort();
}
