/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 *
 */

#ifndef __USER_AUDIO_FEATURE_H__
#define __USER_AUDIO_FEATURE_H__

#define SOF_AUDIO_FEATURE_PARAM_ID	0	/* Use in data ABI header */

enum sof_audio_feature_type {
	SOF_AUDIO_FEATURE_MFCC,
	SOF_AUDIO_FETURE_SOUND_DOSE_MEL,
};

struct sof_audio_feature {
	int64_t stream_time_ms;
	enum sof_audio_feature_type type;
	size_t data_size;
	uint32_t reserved[4];	/**< reserved for future use */
	int32_t data[];
} __attribute__((packed));

#endif /* __USER_AUDIO_FEATURE_H__ */
