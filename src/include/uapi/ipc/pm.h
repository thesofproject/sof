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
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/uapi/ipc/pm.h
 * \brief IPC definitions
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __INCLUDE_UAPI_IPC_PM_H__
#define __INCLUDE_UAPI_IPC_PM_H__

#include <uapi/ipc/header.h>

/*
 * PM
 */

/* PM context element */
struct sof_ipc_pm_ctx_elem {
	struct sof_ipc_hdr hdr;
	uint32_t type;
	uint32_t size;
	uint64_t addr;
} __attribute__((packed));

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
} __attribute__((packed));

/* enable or disable cores - SOF_IPC_PM_CORE_ENABLE */
struct sof_ipc_pm_core_config {
	struct sof_ipc_cmd_hdr hdr;
	uint32_t enable_mask;
} __attribute__((packed));

/* DSP subsystem power states */
enum sof_pm_state {
	SOF_PM_STATE_D0 = 0,	/* D0 only, also initial state */
	SOF_PM_STATE_D3,	/* D3 DSP subsystem is power gated*/
	/* internal FW transitions across D0ix substates incl. D0 are allowed */
	SOF_PM_STATE_D0ix,
};

/* Flags to allow FW for applying internally clock gating policies */
enum sof_pm_cg {
	SOF_PM_CG_ON = 0,	/* Clock gating enabled, */
	SOF_PM_CG_OFF,		/* Clock gating disabled */
};

/* Flags to allow FW for applying power gating policies */
enum sof_pm_pg {
	SOF_PM_PG_ON = 0,	/* Power gating enabled, */
	SOF_PM_PG_OFF,		/* Power gating disabled */
};

/* PM state - SOF_IPC_PM_STATE_SET */
struct sof_ipc_pm_state {
	struct sof_ipc_cmd_hdr hdr;

	uint32_t pm_state;	/**< power state SOF_PM_STATE_D.. */
	/**< flag to disable FW logic for autonomous D0ix<->D0 transitions */
	uint32_t prevent_power_gating;
	/**< flag to disable FW logic for autonomous clock gating */
	uint32_t prevent_clock_gating;
	/**< reserved for future use */
	uint32_t reserved[8];
} __attribute__((packed));

/* PM params info reply - SOF_IPC_PM_STATE_GET */
struct sof_ipc_pm_state_reply {
	struct sof_ipc_reply rhdr;
	uint32_t pm_state;	/**< power state SOF_PM_STATE_D.. */
	/**< flag to disable FW logic for autonomous D0ix<->D0 transitions */
	uint32_t prevent_power_gating;
	/**< flag to disable FW logic for autonomous clock gating */
	uint32_t prevent_clock_gating;
	/**< reserved for future use */
	uint32_t reserved[8];
} __attribute__((packed));

#endif
