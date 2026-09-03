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
#include <ipc4/gateway.h>

#include <sof/compiler_attributes.h>

#include <stddef.h>
#include <stdint.h>

struct audio_stream;
struct cir_buf_source;
struct cir_buf_sink;

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
 * \param source circular-buffer source descriptor, read pointer is not modified
 * \param src_channels number of channels in the source stream
 * \param sink circular-buffer sink descriptor, write pointer is not modified
 * \param sink_channels number of channels in the sink stream
 * \param source_samples number of samples to convert, for remapping -- number of source samples
 * \param chmap channel map for remapping, ignored by non-remapping conversion func
 * \return error code or number of processed samples (source samples in case of remapping).
 */
typedef int (*pcm_converter_func)(const struct cir_buf_source *source,
				  uint32_t src_channels, struct cir_buf_sink *sink,
				  uint32_t sink_channels, size_t source_samples, uint32_t chmap);

/* A channel map that does not perform any remapping. */
#define DUMMY_CHMAP 0x76543210

/**
 * \brief PCM conversion function interface for data in linear buffer
 * \param psrc linear memory region with samples to process
 * \param pdst linear memory region for output
 * \param samples number of samples to convert
 */
typedef void (*pcm_converter_lin_func)(const void *psrc, void *pdst,
				       size_t samples);

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

#if CONFIG_PCM_REMAPPING_CONVERTERS
/** \brief Map of formats with dedicated remap with conversion functions. */
extern const struct pcm_func_map pcm_remap_func_map[];

/** \brief Number of remap with conversion functions. */
extern const size_t pcm_remap_func_count;
#endif

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

#if CONFIG_PCM_REMAPPING_CONVERTERS
/**
 * \brief Retrieves PCM remap with conversion function.
 * \param[in] in Source frame format.
 * \param[in] out Sink frame format.
 */
static inline pcm_converter_func
pcm_get_remap_function(enum sof_ipc_frame in, enum sof_ipc_frame out)
{
	int i;

	for (i = 0; i < pcm_remap_func_count; i++) {
		if (in != pcm_remap_func_map[i].source)
			continue;
		if (out != pcm_remap_func_map[i].sink)
			continue;

		return pcm_remap_func_map[i].func;
	}

	return NULL;
}
#endif

/** \brief PCM conversion functions mapfor different size of valid bit and container. */
struct pcm_func_vc_map {
	enum sof_ipc_frame source;	/**< source frame container format */
	enum sof_ipc_frame valid_src_bits;	/**< source frame format */
	enum sof_ipc_frame sink;	/**< sink frame container format */
	enum sof_ipc_frame valid_sink_bits;	/**< sink frame format */

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

		return pcm_func_vc_map[i].func;
	}

	return NULL;
}

/**
 * \brief Convert data from circular buffer using converter working on linear
 *	  memory space
 * \param source circular-buffer source descriptor, read pointer is not modified
 * \param s_size_in source sample size in bytes
 * \param sink circular-buffer sink descriptor, write pointer is not modified
 * \param s_size_out sink sample size in bytes
 * \param samples number of samples to convert
 * \param converter core conversion function working on linear memory regions
 * \return error code or number of processed samples
 */
int pcm_convert_as_linear(const struct cir_buf_source *source, size_t s_size_in,
			  struct cir_buf_sink *sink, size_t s_size_out,
			  size_t samples, pcm_converter_lin_func converter);

/* Copy stream without conversion, typed by sample size in bytes. chmap is ignored. */
int just_copy_1b(const struct cir_buf_source *source, uint32_t src_channels,
		 struct cir_buf_sink *sink, uint32_t sink_channels,
		 size_t samples, uint32_t chmap);
int just_copy_2b(const struct cir_buf_source *source, uint32_t src_channels,
		 struct cir_buf_sink *sink, uint32_t sink_channels,
		 size_t samples, uint32_t chmap);
int just_copy_3b(const struct cir_buf_source *source, uint32_t src_channels,
		 struct cir_buf_sink *sink, uint32_t sink_channels,
		 size_t samples, uint32_t chmap);
int just_copy_4b(const struct cir_buf_source *source, uint32_t src_channels,
		 struct cir_buf_sink *sink, uint32_t sink_channels,
		 size_t samples, uint32_t chmap);

#endif /* __SOF_AUDIO_PCM_CONVERTER_H__ */
