/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Realtek Semiconductor Corp. All rights reserved.
 *
 * Author: Ming Jen Tai <mingjen_tai@realtek.com>
 */

#ifndef __SOF_AUDIO_RTNR_RTNR_H__
#define __SOF_AUDIO_RTNR_RTNR_H__

#include <stdint.h>
#include <sof/platform.h>
#include <ipc/stream.h>

/**
 * \brief Type definition for the RTNR processing function
 *
 */
typedef void (*rtnr_func)(struct comp_dev *dev,
			 const struct audio_stream **sources,
			 struct audio_stream *sink,
			 int frames);

/* RTNR component private data */
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
	rtnr_func rtnr_func; /** Processing function */
	void *rtk_agl;
};

/** \brief RTNR processing functions map item. */
struct rtnr_func_map {
	enum sof_ipc_frame fmt; /**< source frame format */
	rtnr_func func; /**< processing function */
};

/** \brief Map of formats with dedicated processing functions. */
extern const struct rtnr_func_map rtnr_fnmap[];

/** \brief Number of processing functions. */
extern const size_t rtnr_fncount;

/**
 * \brief Retrieves an RTNR processing function matching
 *	  the source buffer's frame format.
 * \param fmt the frames' format of the source and sink buffers
 */
static inline rtnr_func rtnr_find_func(enum sof_ipc_frame fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < rtnr_fncount; i++) {
		if (fmt == rtnr_fnmap[i].fmt)
			return rtnr_fnmap[i].func;
	}

	return NULL;
}

void myPrintf(int a, int b, int c, int d, int e);
void *rtk_rballoc(unsigned int flags, unsigned int caps, unsigned int bytes);
void rtk_rfree(void *ptr);
#endif /* __SOF_AUDIO_RTNR_RTNR_H__ */
