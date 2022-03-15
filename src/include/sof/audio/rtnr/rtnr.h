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

/* for RTNR internal use */
struct audio_stream_rtnr {
	/* runtime data */
	uint32_t size;	/**< Runtime buffer size in bytes (period multiple) */
	uint32_t avail;	/**< Available bytes for reading */
	uint32_t free;	/**< Free bytes for writing */
	void *w_ptr;	/**< Buffer write pointer */
	void *r_ptr;	/**< Buffer read position */
	void *addr;	/**< Buffer base address */
	void *end_addr;	/**< Buffer end address */

	/* runtime stream params */
	enum sof_ipc_frame frame_fmt;	/**< Sample data format */

	uint32_t rate;		/**< Number of data frames per second [Hz] */
	uint16_t channels;	/**< Number of samples in each frame */

	bool overrun_permitted; /**< indicates whether overrun is permitted */
	bool underrun_permitted; /**< indicates whether underrun is permitted */
};

typedef void (*rtnr_func)(struct comp_dev *dev,
						  struct audio_stream_rtnr **sources,
						  struct audio_stream_rtnr *sink,
						  int frames);

#define RTNR_MAX_SOURCES		1 /* Microphone stream */

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
	rtnr_func rtnr_func; /** Processing function */
	void *rtk_agl;
	struct audio_stream_rtnr sources_stream[RTNR_MAX_SOURCES];
	struct audio_stream_rtnr sink_stream;
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
