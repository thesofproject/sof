/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

#ifndef __SOF_AUDIO_CHANNEL_MAP_H__
#define __SOF_AUDIO_CHANNEL_MAP_H__

#include <ipc/channel_map.h>
#include <sof/common.h>
#include <stdint.h>

/* Returns the size of a channel map in bytes */
static inline uint32_t chmap_get_size(struct sof_ipc_channel_map *chmap)
{
	return sizeof(*chmap) + (popcount(chmap->ch_mask) *
				 sizeof(*chmap->ch_coeffs));
}

/* Returns a channel map valid for a given index in a given stream map */
struct sof_ipc_channel_map *chmap_get(struct sof_ipc_stream_map *smap,
				      int index);

#endif /* __SOF_AUDIO_CHANNEL_MAP_H__ */
