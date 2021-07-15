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
	int ref_shift;
	bool ref_32bits;
	bool ref_active;
	rtnr_func rtnr_func; /** Processing function */
	void *rtk_agl;
};

void myPrintf(int a, int b, int c, int d, int e);
void *rtk_rballoc(unsigned int flags, unsigned int caps, unsigned int bytes);
void rtk_rfree(void *ptr);

#endif /* __SOF_AUDIO_RTNR_RTNR_H__ */
