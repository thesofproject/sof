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
#include <user/rtnr.h>

/**
 * \brief Type definition for the RTNR processing function
 *
 */
typedef void (*rtnr_func)(struct comp_dev *dev,
						  const struct audio_stream **sources,
						  struct audio_stream *sink,
						  int frames);

typedef int (*rtnr_copy_func)(struct comp_dev *dev);

/* RTNR component private data */
struct comp_data {
	struct comp_data_blob_handler *model_handler;
	struct sof_rtnr_config *config;      /**< pointer to setup blob */
	enum sof_ipc_frame source_format;
	enum sof_ipc_frame sink_format;
	enum sof_ipc_frame ref_format;
	int source_channel;
	int reference_channel;
	uint32_t sink_rate;
	uint32_t source_rate;
	bool process_enable;
	uint32_t process_sample_rate;
	int ref_shift;
	bool ref_32bits;
	bool ref_active;
	rtnr_copy_func rtnr_copy_func; /** Processing or passthrough */
	rtnr_func rtnr_func; /** Processing function */
	void *rtk_agl;
};

/* Called by the processing library for debugging purpose */
void rtnr_printf(int a, int b, int c, int d, int e);

/*
 * Called by the processing library to redirect memory allocation/deallocation to
 * sof memory APIs
 */
void *rtk_rballoc(unsigned int flags, unsigned int caps, unsigned int bytes);
void rtk_rfree(void *ptr);

#endif /* __SOF_AUDIO_RTNR_RTNR_H__ */
