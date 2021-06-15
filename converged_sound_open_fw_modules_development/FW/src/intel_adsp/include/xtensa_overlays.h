// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
 /*! file xtensa_overlays.h
  * The source code of xtensa_overlays.h is provided for building modules with
  * non xtensa toolschains.
  * */


void cpu_write_threadptr(int threadptr);
int cpu_read_threadptr(void);

#define WTHREADPTR(threadptr) cpu_write_threadptr((threadptr))
#define RTHREADPTR() cpu_read_threadptr()
