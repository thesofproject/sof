/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 - 2023 Intel Corporation. All rights reserved.
 */

/**
 * \file include/sof/audio/audio_stream_hw.h
 * \brief Synchronisation of an audio stream with the state of its DMA buffer
 *
 * Kept separate from sof/audio/audio_stream.h because it needs the Zephyr
 * DMA driver definitions, which most users of the audio stream API do not.
 * Only available in Zephyr builds.
 */

#ifndef __SOF_AUDIO_AUDIO_STREAM_HW_H__
#define __SOF_AUDIO_AUDIO_STREAM_HW_H__

#include <sof/audio/audio_stream.h>
#include <stdint.h>

#ifdef __ZEPHYR__

#include <zephyr/drivers/dma.h>

/**
 * Replicates the hardware state of the DMA buffer as a struct audio_stream.
 * @param stream Stream to update.
 * @param dma_status DMA buffer hardware state (struct dma_status from Zephyr).
 */
static inline void audio_stream_sync_to_hw(struct audio_stream *stream,
					   const struct dma_status *dma_status)
{
	uintptr_t buf_start = (uintptr_t)audio_stream_get_addr(stream);

	/* Note: It is assumed here that dma_status values are reported as bytes. However,
	 * this is actually platform-specific. Although unlikely, they could be, for example,
	 * words on some particular platform, and changes should be made here to address
	 * such case.
	 */

	assert(dma_status->write_position < audio_stream_get_size(stream));
	assert(dma_status->read_position < audio_stream_get_size(stream));
	assert(dma_status->pending_length <= audio_stream_get_size(stream));
	assert(dma_status->free <= audio_stream_get_size(stream));

	audio_stream_set_wptr(stream, (void *)(buf_start + dma_status->write_position));
	audio_stream_set_rptr(stream, (void *)(buf_start + dma_status->read_position));
	audio_stream_set_avail(stream, dma_status->pending_length);
	audio_stream_set_free(stream, dma_status->free);
}

#endif /* __ZEPHYR__ */

#endif /* __SOF_AUDIO_AUDIO_STREAM_HW_H__ */
