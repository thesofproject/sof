// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// These contents may have been developed with support from one or more
// Intel-operated generative artificial intelligence solutions.
//
// Minimal SOF context for standalone ztest applications that link SOF code
// calling sof_get(), for example src/audio/component.c, but do not link
// src/init/init.c. The context is created on first use and only its component
// driver list is initialised, which is what buffer_new() and the component
// infrastructure need.

#include <rtos/sof.h>
#include <sof/audio/component_ext.h>

#include <stdbool.h>

static struct sof sof_context;
static bool sof_context_initialized;

/**
 * @brief Return the minimal SOF context used by ztest unit suites.
 * @return Pointer to the singleton test SOF context.
 */
struct sof *sof_get(void)
{
	if (!sof_context_initialized) {
		sys_comp_init(&sof_context);
		sof_context_initialized = true;
	}

	return &sof_context;
}
