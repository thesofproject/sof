/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Google LLC.
 *
 * Author: Eddy Hsu <eddyhsu@google.com>
 */
#ifndef __SOF_AUDIO_GOOGLE_GOOGLE_CTC_AUDIO_PROCESSING_H__
#define __SOF_AUDIO_GOOGLE_GOOGLE_CTC_AUDIO_PROCESSING_H__

#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <rtos/init.h>

#include <google_ctc_audio_processing.h>

struct google_ctc_audio_processing_comp_data;

typedef void (*ctc_func)(struct google_ctc_audio_processing_comp_data *cd,
			 const struct audio_stream *source,
			 struct audio_stream *sink,
			 struct input_stream_buffer *input_buffers,
			 struct output_stream_buffer *output_buffers,
			 uint32_t frames);

struct google_ctc_audio_processing_comp_data {
	float *input;
	float *output;
	uint32_t input_samples;
	uint32_t next_avail_output_samples;
	uint32_t chunk_frames;
	GoogleCtcAudioProcessingState *state;
	struct comp_data_blob_handler *tuning_handler;
	bool enabled;
	bool reconfigure;
	ctc_func ctc_func;
};

struct google_ctc_config {
	/* size of the whole ctc config */
	uint32_t size;

	/* reserved */
	uint32_t reserved[4];

	uint32_t data[];
} __packed;

#define CTC_BLOB_DATA_SIZE 4100
#define CTC_BLOB_CONFIG_SIZE (sizeof(struct google_ctc_config) + CTC_BLOB_DATA_SIZE)

int ctc_set_config(struct processing_module *mod, uint32_t param_id,
		   enum module_cfg_fragment_position pos,
		   uint32_t data_offset_size,
		   const uint8_t *fragment,
		   size_t fragment_size, uint8_t *response,
		   size_t response_size);

int ctc_get_config(struct processing_module *mod,
		   uint32_t param_id, uint32_t *data_offset_size,
		   uint8_t *fragment, size_t fragment_size);

#endif  // __SOF_AUDIO_GOOGLE_GOOGLE_CTC_AUDIO_PROCESSING_H__
