/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

#ifndef __LIBRARY_SCHEDULE_HOST_SCHEDULE_H__
#define __LIBRARY_SCHEDULE_HOST_SCHEDULE_H__

#include <pthread.h>

int host_pin_thread_to_dsp_core(pthread_t thread, int dsp_core);

#endif /* __LIBRARY_SCHEDULE_HOST_SCHEDULE_H__ */
