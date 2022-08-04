// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>

#include <ipc4/copier.h>

#ifdef COPIER_GENERIC

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <sof/common.h>
#include <stddef.h>
#include <errno.h>
#include <stdint.h>

LOG_MODULE_REGISTER(copier_generic, CONFIG_SOF_LOG_LEVEL);

int apply_attenuation(struct comp_dev *dev, struct copier_data *cd,
		      struct comp_buffer __sparse_cache *sink, int frame)
{
	int i;
	int n;
	int nmax;
	int remaining_samples = frame * sink->stream.channels;
	int32_t *dst = sink->stream.r_ptr;

	/* only support attenuation in format of 32bit */
	switch (sink->stream.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		comp_err(dev, "16bit sample isn't supported by attenuation");
		return -EINVAL;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		while (remaining_samples) {
			nmax = audio_stream_samples_without_wrap_s32(&sink->stream, dst);
			n = MIN(remaining_samples, nmax);
			for (i = 0; i < n; i++) {
				*dst >>= cd->attenuation;
				dst++;
			}
			remaining_samples -= n;
			dst = audio_stream_wrap(&sink->stream, dst);
		}

		return 0;
	default:
		comp_err(dev, "unsupported format %d for attenuation", sink->stream.frame_fmt);
		return -EINVAL;
	}
}

#endif
