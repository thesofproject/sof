/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/ipc/pm.h
 * \brief IPC definitions
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __IPC_PM_H__
#define __IPC_PM_H__

#include <ipc/header.h>
#include <sof/common.h>

/*
 * PM
 */

/* PM context element */
struct sof_ipc_pm_ctx_elem {
	struct sof_ipc_hdr hdr;
	uint32_t type;
	uint32_t size;
	uint64_t addr;
} __packed;

/*
 * PM context - SOF_IPC_PM_CTX_SAVE, SOF_IPC_PM_CTX_RESTORE,
 * SOF_IPC_PM_CTX_SIZE
 */
struct sof_ipc_pm_ctx {
	struct sof_ipc_cmd_hdr hdr;
	struct sof_ipc_host_buffer buffer;
	uint32_t num_elems;
	uint32_t size;

	/* reserved for future use */
	uint32_t reserved[8];

	struct sof_ipc_pm_ctx_elem elems[];
} __packed;

/* enable or disable cores - SOF_IPC_PM_CORE_ENABLE */
struct sof_ipc_pm_core_config {
	struct sof_ipc_cmd_hdr hdr;
	uint32_t enable_mask;
} __packed;

#endif /* __IPC_PM_H__ */
