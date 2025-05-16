/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 - 2023 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *	   Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *	   Keyon Jie <yang.jie@linux.intel.com>
 *	   Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __MODULE_AUDIO_FORMAT_H__
#define __MODULE_AUDIO_FORMAT_H__

#include <stdint.h>
#include "../ipc/stream.h"

static inline uint32_t get_sample_bytes(enum sof_ipc_frame fmt)
{
	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
		return 2;
	case SOF_IPC_FRAME_S24_3LE:
		return 3;
	case SOF_IPC_FRAME_U8:
	case SOF_IPC_FRAME_A_LAW:
	case SOF_IPC_FRAME_MU_LAW:
		return 1;
	default:
		return 4;
	}
}

static inline uint32_t get_sample_bitdepth(enum sof_ipc_frame fmt)
{
	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
		return 16;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S24_3LE:
		return 24;
	case SOF_IPC_FRAME_U8:
	case SOF_IPC_FRAME_A_LAW:
	case SOF_IPC_FRAME_MU_LAW:
		return 8;
	default:
		return 32;
	}
}

static inline uint32_t get_frame_bytes(enum sof_ipc_frame fmt, uint32_t channels)
{
	return get_sample_bytes(fmt) * channels;
}

#endif /* __MODULE_AUDIO_FORMAT_H__ */
