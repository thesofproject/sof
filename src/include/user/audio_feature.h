/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 *
 */

#ifndef __USER_AUDIO_FEATURE_H__
#define __USER_AUDIO_FEATURE_H__

/** \brief Audio feature data types. */
enum sof_audio_feature_type {
	SOF_AUDIO_FEATURE_MFCC,			/**< For Mel Frequency Cepstral Coefficients */
	SOF_AUDIO_FEATURE_SOUND_DOSE_MEL,	/**< For Sound Dose MEL (loudness) values */
};

/** \brief Header for audio features data. */
struct sof_audio_feature {
	uint64_t stream_time_us;		/**< Timestamp, relative time in microseconds */
	enum sof_audio_feature_type type;	/**< Type of audio feature, as above*/
	uint32_t num_audio_features;		/**< Number of audio feature structs in data */
	size_t data_size;			/**< Size of data without this header */
	uint32_t reserved[4];			/**< Reserved for future use */
	int32_t data[];				/**< Start of data */
} __attribute__((packed));

#endif /* __USER_AUDIO_FEATURE_H__ */
