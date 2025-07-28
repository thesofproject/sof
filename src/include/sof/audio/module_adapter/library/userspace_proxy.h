/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __SOF_AUDIO_USERSPACE_PROXY_H__
#define __SOF_AUDIO_USERSPACE_PROXY_H__

#if CONFIG_USERSPACE
#include <stdint.h>
#include <stdbool.h>

#include <zephyr/kernel.h>

/* Processing module structure fields needed for user mode */
struct userspace_context {
	struct k_mem_domain *comp_dom;			/* Module specific memory domain	*/
};

#endif /* CONFIG_USERSPACE */

#endif /* __SOF_AUDIO_USERSPACE_PROXY_H__ */
