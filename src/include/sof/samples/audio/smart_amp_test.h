/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

#ifndef __SOF_AUDIO_SMART_AMP_H__
#define __SOF_AUDIO_SMART_AMP_H__

#include <sof/platform.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>

#define SMART_AMP_MAX_STREAM_CHAN   8

/** IPC blob types */
#define SOF_SMART_AMP_CONFIG	0
#define SOF_SMART_AMP_MODEL	1

struct smart_amp_model_data {
	uint32_t data_size;
	void *data;
	uint32_t crc;
	uint32_t data_pos;
};

typedef int(*smart_amp_proc)(struct comp_dev *dev,
			     const struct audio_stream __sparse_cache *source,
			     const struct audio_stream __sparse_cache *sink, uint32_t frames,
			     int8_t *chan_map);

/* Each channel map specifies which channel from input (buffer between host
 * and smart amp - source_chan_map[] or feedback buffer between smart amp and
 * demux - feedback_chan_map[]) will be copied to specific smart amp output
 * channel. Value -1 means that for this output channel we will not take any
 * channel from specific input.
 *
 * E.g. assuming that a smart amplifier input stream is configured
 * with channels parameter set to 2, feedback stream with channels set to 8
 * and smart amplifier output stream with channels set to 4 (smart amplifier
 * converts stream from 2 to 4 channels) and source/feedback_ch_map's are as
 * follows:
 *
 * source_ch_map = [0, 1, -1, -1, -1 ,-1 ,-1, -1]
 * feedback_ch_map = [-1, -1, 0, 1, -1, -1, -1, -1]
 *
 * As a result smart amplifier test component will procces source and feedback
 * streams in following way:
 *
 *
 *
 * PLAYBACK
 * STREAM      +---+
 *             | 0 +------------+
 *             +---+            |           SMART AMPLIFIER
 *             | 1 +---------+  |           OUTPUT
 *             +---+         |  |   +---+
 *                           |  +-->+ 0 |
 *                           |      +---+
 *                           +----->+ 1 |
 *                                  +---+
 *             +---+           +--->+ 2 |
 * FEEDBACK    | 0 +-----------+    +---+
 * STREAM      +---+             +->| 3 |
 *             | 1 +-------------+  +---+
 *             +---+                | 4 |
 *             | 2 |                +---+
 *             +---+                | 5 |
 *             | 3 |                +---+
 *             +---+                | 6 |
 *             | 4 |                +---+
 *             +---+                | 7 |
 *             | 5 |                +---+
 *             +---+
 *             | 6 |
 *             +---+
 *             | 7 |
 *             +---+
 *
 */

struct sof_smart_amp_config {
	uint32_t size;
	uint32_t feedback_channels;
	int8_t source_ch_map[PLATFORM_MAX_CHANNELS];
	int8_t feedback_ch_map[PLATFORM_MAX_CHANNELS];
};

#endif /* __SOF_AUDIO_SMART_AMP_H__ */
