/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_AUDIO_AEC_AEC_H__
#define __SOF_AUDIO_AEC_AEC_H__

#include <stdint.h>
#include <sof/platform.h>
#include <ipc/stream.h>

/**
 * \brief Type definition for the AEC processing function
 *
 */
typedef void (*aec_func)(struct comp_dev *dev,
			 const struct audio_stream **sources,
			 struct audio_stream *sink,
			 int frames);

/* AEC component private data */
struct comp_data {
	struct comp_data_blob_handler *model_handler;
	enum sof_ipc_frame source_format;
	enum sof_ipc_frame sink_format;
	enum sof_ipc_frame ref_format;
	int source_channel;
	int reference_channel;
	int count;
	int ref_shift;
	bool ref_32bits;
	bool ref_active;
	aec_func aec_func; /** Processing function */
};

/** \brief AEC processing functions map item. */
struct aec_func_map {
	enum sof_ipc_frame fmt; /**< source frame format */
	aec_func func; /**< processing function */
};

/** \brief Map of formats with dedicated processing functions. */
extern const struct aec_func_map aec_fnmap[];

/** \brief Number of processing functions. */
extern const size_t aec_fncount;

/**
 * \brief Retrieves an AEC processing function matching
 *	  the source buffer's frame format.
 * \param fmt the frames' format of the source and sink buffers
 */
static inline aec_func aec_find_func(enum sof_ipc_frame fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < aec_fncount; i++) {
		if (fmt == aec_fnmap[i].fmt)
			return aec_fnmap[i].func;
	}

	return NULL;
}

#endif /* __SOF_AUDIO_AEC_AEC_H__ */
