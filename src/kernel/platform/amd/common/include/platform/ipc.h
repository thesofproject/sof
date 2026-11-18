/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023, 2026 AMD. All rights reserved.
 *
 *Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *		SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>
 *      Sivasubramanian <sravisar@amd.com>
 */
#ifndef _COMMON_IPC_HEADER
#define _COMMON_IPC_HEADER

#include <stdint.h>
#include <platform/platform.h>

#define IRQ_NUM_EXT_LEVEL3 3
uint32_t sof_ipc_host_status(void);
uint32_t sof_ipc_host_ack_flag(void);
void sof_ipc_host_ack_clear(void);
uint32_t sof_ipc_host_msg_flag(void);
void sof_ipc_host_msg_clear(void);
void sof_ipc_dsp_ack_set(void);
uint32_t sof_ipc_dsp_status(void);
void sof_ipc_dsp_msg_set(void);
void amd_irq_handler(void *arg);
#endif
