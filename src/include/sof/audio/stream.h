/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_AUDIO_STREAM_H__
#define __SOF_AUDIO_STREAM_H__

struct sof_ipc_pcm_params;
struct sof_ipc_vorbis_params;

enum stream_type {
	STREAM_TYPE_PCM		= 0,
	STREAM_TYPE_VORBIS	= 1,
};

struct stream_params {
	enum stream_type type;
	union {
		struct sof_ipc_pcm_params *pcm;
		struct sof_ipc_vorbis_params *vorbis;
	};
};

#endif /* __SOF_AUDIO_STREAM_H__ */
