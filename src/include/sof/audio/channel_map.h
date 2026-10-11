/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

/**
 * @file sof/audio/channel_map.h
 * @brief Audio stream channel map lookup and sizing helpers.
 * @ingroup audio_pipeline_api
 */

#ifndef __SOF_AUDIO_CHANNEL_MAP_H__
#define __SOF_AUDIO_CHANNEL_MAP_H__

#include <ipc/channel_map.h>
#include <sof/common.h>
#include <stdint.h>

/**
 * @brief Returns the size of an IPC channel map structure in bytes.
 * @param chmap Pointer to the channel map structure.
 * @return Total size in bytes including dynamic channel coefficients.
 */
static inline uint32_t chmap_get_size(struct sof_ipc_channel_map *chmap)
{
	return sizeof(*chmap) + (popcount(chmap->ch_mask) *
				 sizeof(*chmap->ch_coeffs));
}

/**
 * @brief Returns a channel map for a given channel index in a stream map.
 * @param smap Pointer to the stream map structure.
 * @param index Channel index.
 * @return Pointer to matching channel map structure, or NULL if not found.
 */
struct sof_ipc_channel_map *chmap_get(struct sof_ipc_stream_map *smap,
				      int index);

#endif /* __SOF_AUDIO_CHANNEL_MAP_H__ */
