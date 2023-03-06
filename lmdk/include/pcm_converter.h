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

#include <../include/ipc/stream.h>
#include <../include/ipc4/gateway.h>

#include <stddef.h>
#include <stdint.h>

struct audio_stream;

#if __XCC__
#include <xtensa/config/core-isa.h>
#endif

#ifndef UNIT_TEST
#if __XCC__ && XCHAL_HAVE_HIFI3 && CONFIG_FORMAT_CONVERT_HIFI3
#define PCM_CONVERTER_HIFI3
#else
#define PCM_CONVERTER_GENERIC
#endif
#endif /* UNIT_TEST */

/**
 * \brief PCM conversion function interface for data in circular buffer
 * \param source buffer with samples to process, read pointer is not modified
 * \param ioffset offset to first sample in source stream
 * \param sink output buffer, write pointer is not modified
 * \param ooffset offset to first sample in sink stream
 * \param samples number of samples to convert
 * \return error code or number of processed samples.
 */
typedef int (*pcm_converter_func)(const struct audio_stream __attribute__((address_space(__cache))) *source,
				  uint32_t ioffset, struct audio_stream __attribute__((address_space(__cache))) *sink,
				  uint32_t ooffset, uint32_t samples);

/**
 * \brief PCM conversion function interface for data in linear buffer
 * \param psrc linear memory region with samples to process
 * \param pdst linear memory region for output
 * \param samples number of samples to convert
 */
typedef void (*pcm_converter_lin_func)(const void *psrc, void *pdst,
				       uint32_t samples);

/** \brief PCM conversion functions map. */
struct pcm_func_map {
	enum sof_ipc_frame source;	/**< source frame format */
	enum sof_ipc_frame sink;	/**< sink frame format */
	pcm_converter_func func; /**< PCM conversion function */
};

/** \brief Map of formats with dedicated conversion functions. */
extern const struct pcm_func_map pcm_func_map[];

/** \brief Number of conversion functions. */
extern const size_t pcm_func_count;

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

/** \brief PCM conversion functions mapfor different size of valid bit and container. */
struct pcm_func_vc_map {
	enum sof_ipc_frame source;	/**< source frame container format */
	enum sof_ipc_frame valid_src_bits;	/**< source frame format */
	enum sof_ipc_frame sink;	/**< sink frame container format */
	enum sof_ipc_frame valid_sink_bits;	/**< sink frame format */
	uint32_t type;	/**< gateway type */
	enum ipc4_direction_type direction;	/**< support playback, capture or both */
	pcm_converter_func func; /**< PCM conversion function */
};

/** \brief Map of formats with dedicated conversion functions. */
extern const struct pcm_func_vc_map pcm_func_vc_map[];

/** \brief Number of conversion functions. */
extern const size_t pcm_func_vc_count;

/**
 * \brief Retrieves PCM conversion function for different container size.
 * \param in_bits is source container format.
 * \param valid_in_bits is source valid sample format.
 * \param out_bits is sink container format.
 * \param valid_out_bits is sink valid sample format.
 * \param type is gateway type
 * \param dir is playback or capture
 */
static inline pcm_converter_func
pcm_get_conversion_vc_function(enum sof_ipc_frame in_bits,
			       enum sof_ipc_frame valid_in_bits,
			       enum sof_ipc_frame out_bits,
			       enum sof_ipc_frame valid_out_bits,
			       enum ipc4_gateway_type type,
			       enum ipc4_direction_type dir)
{
	uint32_t i;

	for (i = 0; i < pcm_func_vc_count; i++) {
		if (in_bits != pcm_func_vc_map[i].source)
			continue;
		if (valid_in_bits != pcm_func_vc_map[i].valid_src_bits)
			continue;
		if (out_bits != pcm_func_vc_map[i].sink)
			continue;
		if (valid_out_bits != pcm_func_vc_map[i].valid_sink_bits)
			continue;

		if (!(type & pcm_func_vc_map[i].type))
			continue;

		if (!(dir & pcm_func_vc_map[i].direction))
			continue;

		return pcm_func_vc_map[i].func;
	}

	return NULL;
}

/**
 * \brief Convert data from circular buffer using converter working on linear
 *	  memory space
 * \param source buffer with samples to process, read pointer is not modified
 * \param ioffset offset to first sample in source stream
 * \param sink output buffer, write pointer is not modified
 * \param ooffset offset to first sample in sink stream
 * \param samples number of samples to convert
 * \param converter core conversion function working on linear memory regions
 * \return error code or number of processed samples
 */
int pcm_convert_as_linear(const struct audio_stream __attribute__((address_space(__cache))) *source, uint32_t ioffset,
			  struct audio_stream __attribute__((address_space(__cache))) *sink, uint32_t ooffset,
			  uint32_t samples, pcm_converter_lin_func converter);

#endif /* __SOF_AUDIO_PCM_CONVERTER_H__ */
