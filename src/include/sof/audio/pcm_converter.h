/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file audio/pcm_converter.h
 * \brief PCM converter header file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __SOF_AUDIO_PCM_CONVERTER_H__
#define __SOF_AUDIO_PCM_CONVERTER_H__

#include <ipc/stream.h>
#include <config.h>
#include <stddef.h>
#include <stdint.h>

struct audio_stream;

#define PCM_CONVERTER_GENERIC

#if __XCC__

#include <xtensa/config/core-isa.h>

#if XCHAL_HAVE_HIFI3 && CONFIG_FORMAT_CONVERT_HIFI3

#undef PCM_CONVERTER_GENERIC
#define PCM_CONVERTER_HIFI3

#endif

#endif

/**
 * \brief PCM conversion function interface for data in circular buffer
 * \param source buffer with samples to process, read pointer is not modified
 * \param ioffset offset to first sample in source stream
 * \param sink output buffer, write pointer is not modified
 * \param ooffset offset to first sample in sink stream
 * \param samples number of samples to convert
 */
typedef void (*pcm_converter_func)(const struct audio_stream *source,
				   uint32_t ioffset, struct audio_stream *sink,
				   uint32_t ooffset, uint32_t samples);

/** \brief PCM conversion functions map. */
struct pcm_func_map {
	enum sof_ipc_frame source;	/**< source frame format */
	enum sof_ipc_frame sink;	/**< sink frame format */
	pcm_converter_func func; /**< PCM conversion function */
};

/** \brief Map of formats with dedicated conversion functions. */
extern const struct pcm_func_map pcm_func_map[];

/** \brief Number of conversion functions. */
extern const uint32_t pcm_func_count;

/**
 * \brief Retrieves PCM conversion function.
 * \param[in] in Source frame format.
 * \param[in] out Sink frame format.
 */
static inline pcm_converter_func
pcm_get_conversion_function(enum sof_ipc_frame in,
			    enum sof_ipc_frame out)
{
	uint32_t i;

	for (i = 0; i < pcm_func_count; i++) {
		if (in != pcm_func_map[i].source)
			continue;
		if (out != pcm_func_map[i].sink)
			continue;

		return pcm_func_map[i].func;
	}

	return NULL;
}

#endif /* __SOF_AUDIO_PCM_CONVERTER_H__ */
