// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>
#include <ipc4/copier.h>

#if __XCC__ && (XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4)

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <sof/common.h>
#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include <xtensa/tie/xt_hifi3.h>

int apply_attenuation(struct comp_dev *dev, struct copier_data *cd,
		      struct comp_buffer __sparse_cache *sink, int frame)
{
	int i;
	int n;
	int nmax;
	ae_int32x2 sample;
	ae_valign uu = AE_ZALIGN64();
	ae_valign su = AE_ZALIGN64();
	int remaining_samples = frame * sink->stream.channels;
	uint32_t *dst = sink->stream.r_ptr;
	ae_int32x2 *in = (ae_int32x2 *)dst;
	ae_int32x2 *out = (ae_int32x2 *)dst;

	/* only support attenuation in format of 32bit */
	switch (sink->stream.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		comp_err(dev, "16bit sample isn't supported by attenuation");
		return -EINVAL;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		while (remaining_samples) {
			nmax = audio_stream_samples_without_wrap_s32(&sink->stream, dst);
			in = (ae_int32x2 *)dst;
			out = (ae_int32x2 *)dst;
			uu = AE_LA64_PP(in);
			n = MIN(remaining_samples, nmax);
			for (i = 0; i < n; i += 2) {
				AE_LA32X2_IP(sample, uu, in);
				sample = AE_SRAA32(sample, cd->attenuation);
				AE_SA32X2_IP(sample, su, out);
			}
			AE_SA64POS_FP(su, out);
			remaining_samples -= n;
			dst = audio_stream_wrap(&sink->stream, dst + n);
		}

		return 0;
	default:
		comp_err(dev, "unsupported format %d for attenuation", sink->stream.frame_fmt);
		return -EINVAL;
	}
}
#endif
