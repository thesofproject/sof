/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Google LLC. All rights reserved.
 *
 * Author: Sebastiano Carlucci <scarlucci@google.com>
 */

#ifndef __SOF_AUDIO_DCBLOCK_DCBLOCK_H__
#define __SOF_AUDIO_DCBLOCK_DCBLOCK_H__

#include <stdint.h>
#include <sof/platform.h>
#include <ipc/stream.h>
#include <sof/compiler_info.h>
#include <module/module/base.h>
#include <module/module/interface.h>

struct audio_stream;
struct comp_dev;

struct dcblock_state {
	int32_t x_prev; /**< state variable referring to x[n-1] */
	int32_t y_prev; /**< state variable referring to y[n-1] */
};

/**
 * \brief Type definition for the processing function for the
 * DC Blocking Filter.
 */

struct comp_data;

typedef void (*dcblock_func)(struct comp_data *cd,
			     const struct audio_stream *source,
			     const struct audio_stream *sink,
			     uint32_t frames);

/* DC Blocking Filter component private data */
struct comp_data {
	/**< filters state */
	struct dcblock_state state[PLATFORM_MAX_CHANNELS];

	/** coefficients for the processing function */
	int32_t R_coeffs[PLATFORM_MAX_CHANNELS];

	struct comp_data_blob_handler *model_handler;
	int32_t *config; /**< pointer to setup blob */

	enum sof_ipc_frame source_format;
	enum sof_ipc_frame sink_format;
	dcblock_func dcblock_func; /**< processing function */
};

/** \brief DC Blocking Filter processing functions map item. */
struct dcblock_func_map {
	enum sof_ipc_frame src_fmt; /**< source frame format */
	dcblock_func func; /**< processing function */
};

/** \brief Map of formats with dedicated processing functions. */
extern const struct dcblock_func_map dcblock_fnmap[];

/** \brief Number of processing functions. */
extern const size_t dcblock_fncount;

/**
 * \brief Retrieves a DC Blocking processing function matching
 *	  the source buffer's frame format.
 * \param src_fmt the frames' format of the source buffer
 */
static inline dcblock_func dcblock_find_func(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < dcblock_fncount; i++) {
		if (src_fmt == dcblock_fnmap[i].src_fmt)
			return dcblock_fnmap[i].func;
	}

	return NULL;
}

int dcblock_get_ipc_config(struct processing_module *mod,
			   uint8_t *fragment, size_t fragment_size);
int dcblock_set_ipc_config(struct processing_module *mod,
			   enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			   const uint8_t *fragment, size_t fragment_size);
void dcblock_params(struct processing_module *mod, struct comp_buffer *sourceb,
		    struct comp_buffer *sinkb);

#endif /* __SOF_AUDIO_DCBLOCK_DCBLOCK_H__ */
