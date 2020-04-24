/*
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __INCLUDE_ATOMIC_H_
#define __INCLUDE_ATOMIC_H_

#include <sys/atomic.h>

#define atomic_read	atomic_get
#define atomic_init	atomic_set

#endif
