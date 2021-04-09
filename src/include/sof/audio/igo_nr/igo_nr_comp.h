/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intelligo Technology Inc. All rights reserved.
 *
 * Author: Fu-Yun TSUO <fy.tsuo@intelli-go.com>
 */

#ifndef __SOF_AUDIO_IGO_NR_CONFIG_H__
#define __SOF_AUDIO_IGO_NR_CONFIG_H__

#include <sof/platform.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/igo_nr/igo_lib.h>

#define IGO_FRAME_SIZE (256)
#define IGO_NR_IN_BUF_LENGTH (IGO_FRAME_SIZE)
#define IGO_NR_OUT_BUF_LENGTH (IGO_FRAME_SIZE)

/* IGO_NR component private data */
struct comp_data {
	void *p_handle;
	struct IgoLibInfo igo_lib_info;
	struct IgoLibConfig igo_lib_config;
	struct IgoStreamData igo_stream_data_in;
	struct IgoStreamData igo_stream_data_ref;
	struct IgoStreamData igo_stream_data_out;
	struct comp_data_blob_handler *model_handler;
	struct sof_igo_nr_config config;	    /**< blob data buffer */
	int16_t in[IGO_NR_IN_BUF_LENGTH];	    /**< input samples buffer */
	int16_t out[IGO_NR_IN_BUF_LENGTH];    /**< output samples mix buffer */
	uint16_t in_wpt;
	uint16_t out_rpt;
	bool process_enable[SOF_IPC_MAX_CHANNELS];	/**< set if channel process is enabled */
	bool invalid_param;	/**< sample rate != 16000 */
	uint32_t sink_rate;	/* Sample rate in Hz */
	uint32_t source_rate;	/* Sample rate in Hz */
	uint32_t sink_format;	/* For used PCM sample format */
	uint32_t source_format;	/* For used PCM sample format */
	int32_t source_frames;	/* Nominal # of frames to process at source */
	int32_t sink_frames;	/* Nominal # of frames to process at sink */
	int32_t source_frames_max;	/* Max # of frames to process at source */
	int32_t sink_frames_max;	/* Max # of frames to process at sink */
	int32_t frames;		/* IO buffer length */
	void (*igo_nr_func)(struct comp_data *cd,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    int src_frames,
			    int snk_frames);
};

#endif /* __SOF_AUDIO_IGO_NR_CONFIG_H__ */
