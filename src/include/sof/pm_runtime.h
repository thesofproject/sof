/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file include/sof/pm_runtime.h
 * \brief Runtime power management header file
 * \author Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __INCLUDE_PM_RUNTIME__
#define __INCLUDE_PM_RUNTIME__

#include <sof/lock.h>
#include <sof/trace.h>

/** \brief Power management trace function. */
#define trace_pm(__e)	trace_event(TRACE_CLASS_POWER, __e)
#define tracev_pm(__e)	tracev_event(TRACE_CLASS_POWER, __e)

/** \brief Power management trace value function. */
#define tracev_pm_value(__e)	tracev_value(__e)

/** \brief Runtime power management context */
enum pm_runtime_context {
	PM_RUNTIME_HOST_DMA_L1 = 0,	/**< Host DMA L1 Exit */
};

/** \brief Runtime power management data. */
struct pm_runtime_data {
	spinlock_t lock;	/**< lock mechanism */
	void *platform_data;	/**< platform specific data */
};

/**
 * \brief Initializes runtime power management.
 */
void pm_runtime_init(void);

/**
 * \brief Retrieves power management resource.
 * \param[in] context Type of power management context.
 */
void pm_runtime_get(enum pm_runtime_context context);

/**
 * \brief Releases power management resource.
 * \param[in] context Type of power management context.
 */
void pm_runtime_put(enum pm_runtime_context context);

#endif /* __INCLUDE_PM_RUNTIME__ */
