// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
 /*! file xtensa_overlays.cc
  * The source code of xtensa_overlays.cc is provided for building modules with
  * non xtensa toolschains.
  * */

#include <xtensa_overlays.h>

void cpu_write_threadptr(int threadptr)
{
	__asm__ __volatile__(
		"wur.threadptr %0" : : "a" (threadptr) : "memory");
}

int cpu_read_threadptr(void)
{
	int threadptr;
	__asm__ __volatile__(
		"rur.threadptr %0" : "=a"(threadptr));

	return threadptr;
}
