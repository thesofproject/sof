/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

/**
 * \file include/ipc/header-intel-cavs.h
 * \brief IPC command header for Intel cAVS platforms
 * \author Marcin Maka <marcin.maka@linux.intel.com>
 */

#ifndef __IPC_HEADER_INTEL_CAVS_H__
#define __IPC_HEADER_INTEL_CAVS_H__

#include <rtos/bit.h>

/* Primary register, mapped to
 * - DIPCTDR (HIPCIDR) in sideband IPC (cAVS 1.8+)
 * - DIPCT in cAVS 1.5 IPC
 *
 * Secondary register, mapped to:
 * - DIPCTDD (HIPCIDD) in sideband IPC (cAVS 1.8+)
 * - DIPCTE in cAVS 1.5 IPC
 */

/* Common bits in primary register */

#define CAVS_IPC_RSVD_31		BIT(31)

/** \brief Target, 0 - global message, 1 - message to a module */
#define CAVS_IPC_MSG_TGT		BIT(30)

/** \brief Direction, 0 - request, 1 - response */
#define CAVS_IPC_RSP			BIT(29)

#define CAVS_IPC_TYPE_SHIFT		24
#define CAVS_IPC_TYPE_MASK		MASK(28, 24)
#define CAVS_IPC_TYPE(x)		((x) << CAVS_IPC_TYPE_SHIFT)

/* Bits in primary register for Module messages (CAVS_IPC_MSG_TGT set to 1) */

/** \brief ID of the target module instance */
#define CAVS_IPC_MOD_INSTANCE_ID_MASK	MASK(23, 16)

/** \brief ID of the target module */
#define CAVS_IPC_MOD_ID_MASK		MASK(15, 0)

/* Definition of bits in secondary register are message specific
 * and are defined for each message separately later.
 */

/* Primary register :: type value for Module messages */

#define CAVS_IPC_MOD_INIT_INSTANCE	CAVS_IPC_TYPE(0x0U)
#define CAVS_IPC_MOD_CFG_GET		CAVS_IPC_TYPE(0x1U)
#define CAVS_IPC_MOD_CFG_SET		CAVS_IPC_TYPE(0x2U)
#define CAVS_IPC_MOD_LARGE_CFG_GET	CAVS_IPC_TYPE(0x3U)
#define CAVS_IPC_MOD_LARGE_CFG_SET	CAVS_IPC_TYPE(0x4U)
#define CAVS_IPC_MOD_BIND		CAVS_IPC_TYPE(0x5U)
#define CAVS_IPC_MOD_UNBIND		CAVS_IPC_TYPE(0x6U)
#define CAVS_IPC_MOD_SET_DX		CAVS_IPC_TYPE(0x7U)
#define CAVS_IPC_MOD_SET_D0IX		CAVS_IPC_TYPE(0x8U)
#define CAVS_IPC_MOD_ENTER_RESTORE	CAVS_IPC_TYPE(0x9U)
#define CAVS_IPC_MOD_EXIT_RESTORE	CAVS_IPC_TYPE(0xaU)
#define CAVS_IPC_MOD_DELETE_INSTANCE	CAVS_IPC_TYPE(0xbU)
#define CAVS_IPC_MOD_NOTIFICATION	CAVS_IPC_TYPE(0xcU)

/* Secondary register bits for Module::SetD0iX
 * ( Primary
 *     tgt = 1 (module message)
 *     rsp = 0 (request)
 *     type = CAVS_IPC_MOD_SET_D0IX
 * )
 */

/** \brief Valid bits for SetD0ix */
#define CAVS_IPC_MOD_SETD0IX_BIT_MASK	MASK(7, 0)

/** \brief Disable DMA tracing (0 - keep tracing, 1 - to disable DMA trace) */
#define CAVS_IPC_MOD_SETD0IX_NO_TRACE	BIT(4)

/** \brief Prevent clock gating (0 - cg allowed, 1 - DSP clock always on) */
#define CAVS_IPC_MOD_SETD0IX_PCG	BIT(3)

/** \brief Prevent power gating (0 - D0ix transitions allowed) */
#define CAVS_IPC_MOD_SETD0IX_PPG	BIT(2)

/** \brief Streaming active */
#define CAVS_IPC_MOD_SETD0IX_STREAMING	BIT(1)

/** \brief Legacy wake type, unused in cAVS 1.8+ */
#define CAVS_IPC_MOD_SETD0IX_WAKE_TYPE	BIT(0)

#endif /* __IPC_HEADER_INTEL_CAVS_H__ */
