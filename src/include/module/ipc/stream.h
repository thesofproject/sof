/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 - 2023 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *	   Keyon Jie <yang.jie@linux.intel.com>
 *	   Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __MODULE_IPC_STREAM_H__
#define __MODULE_IPC_STREAM_H__

#define SOF_IPC_MAX_CHANNELS			8

/* stream PCM frame format */
enum sof_ipc_frame {
	SOF_IPC_FRAME_S16_LE = 0,
	SOF_IPC_FRAME_S24_4LE,
	SOF_IPC_FRAME_S32_LE,
	SOF_IPC_FRAME_FLOAT,
	/* other formats here */
	SOF_IPC_FRAME_S24_3LE,
	SOF_IPC_FRAME_S24_4LE_MSB,
	SOF_IPC_FRAME_U8,
	SOF_IPC_FRAME_S16_4LE		/* 16-bit in 32-bit container */
};

#endif /* __MODULE_IPC_STREAM_H__ */
