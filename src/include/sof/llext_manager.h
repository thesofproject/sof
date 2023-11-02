/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_LLEXT_MANAGER_H__
#define __SOF_LLEXT_MANAGER_H__

#include <stdint.h>

struct comp_driver;
struct comp_ipc_config;

uintptr_t llext_manager_allocate_module(struct processing_module *proc,
					struct comp_ipc_config *ipc_config,
					const void *ipc_specific_config, const void **buildinfo);
int llext_manager_free_module(struct processing_module *proc,
			      struct comp_ipc_config *ipc_config);

#endif
