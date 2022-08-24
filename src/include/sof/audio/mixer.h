/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifndef __SOF_AUDIO_MIXER_H__
#define __SOF_AUDIO_MIXER_H__

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/platform.h>
#include <stddef.h>
#include <stdint.h>

#ifdef UNIT_TEST
void sys_comp_mixer_init(void);
#endif

#define MIXER_GENERIC

#if defined(__XCC__)
#include <xtensa/config/core-isa.h>

#if XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4
#undef MIXER_GENERIC
#endif

#endif

/**
 * \brief mixer processing function interface
 */
typedef void (*mixer_func)(struct comp_dev *dev, struct audio_stream __sparse_cache *sink,
			   const struct audio_stream __sparse_cache **sources, uint32_t num_sources,
			   uint32_t frames);

/** \brief Volume processing functions map. */
struct mixer_func_map {
	uint16_t frame_fmt;	/**< frame format */
	mixer_func func;	/**< volume processing function */
};

/** \brief Map of formats with dedicated processing functions. */
extern const struct mixer_func_map mixer_func_map[];

/** \brief Number of processing functions. */
extern const size_t mixer_func_count;

/**
 * \brief Retrievies mixer processing function.
 * \param[in,out] dev Mixer base component device.
 * \param[in] sinkb Sink buffer to match against
 */
static inline mixer_func mixer_get_processing_function(struct comp_dev *dev,
						       struct comp_buffer __sparse_cache *sinkb)
{
	int i;

	/* map the volume function for source and sink buffers */
	for (i = 0; i < mixer_func_count; i++) {
		if (sinkb->stream.frame_fmt != mixer_func_map[i].frame_fmt)
			continue;

		return mixer_func_map[i].func;
	}

	return NULL;
}

#endif /* __SOF_AUDIO_MIXER_H__ */
