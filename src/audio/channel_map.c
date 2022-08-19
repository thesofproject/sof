// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <sof/audio/channel_map.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <rtos/bit.h>
#include <sof/common.h>
#include <ipc/channel_map.h>
#include <stdint.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(channel_map, CONFIG_SOF_LOG_LEVEL);

/* ec290e95-4a20-47eb-bbff-d9c888431831 */
DECLARE_SOF_UUID("channel-map", chmap_uuid, 0xec290e95, 0x4a20, 0x47eb,
		 0xbb, 0xff, 0xd9, 0xc8, 0x88, 0x43, 0x18, 0x31);

DECLARE_TR_CTX(chmap_tr, SOF_UUID(chmap_uuid), LOG_LEVEL_INFO);

struct sof_ipc_channel_map *chmap_get(struct sof_ipc_stream_map *smap,
				      int index)
{
	struct sof_ipc_channel_map *chmap = NULL;
	char *mem;
	uint32_t byte = 0;

	if (index >= smap->num_ch_map) {
		tr_err(&chmap_tr, "chmap_get(): index %d out of bounds %d",
		       index, smap->num_ch_map);

		return NULL;
	}

	do {
		mem = (char *)smap->ch_map + byte;
		chmap = (struct sof_ipc_channel_map *)mem;
		byte += chmap_get_size(chmap);
	} while (index-- > 0);

	return chmap;
}
