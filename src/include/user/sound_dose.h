/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 *
 */

#include <kernel/abi.h>
#include <kernel/header.h>
#include <user/audio_feature.h>

#ifndef __USER_SOUND_DOSE_H__
#define __USER_SOUND_DOSE_H__

#define SOF_SOUND_DOSE_SETUP_PARAM_ID		0
#define SOF_SOUND_DOSE_VOLUME_PARAM_ID		1
#define SOF_SOUND_DOSE_GAIN_PARAM_ID		2
#define SOF_SOUND_DOSE_PAYLOAD_PARAM_ID		3

#define SOF_SOUND_DOSE_SENS_MIN_DB	(-10 * 100)	/* -10 to +130 dB */
#define SOF_SOUND_DOSE_SENS_MAX_DB	(130 * 100)
#define SOF_SOUND_DOSE_VOLUME_MIN_DB	(-100 * 100)	/* -100 to +40 dB */
#define SOF_SOUND_DOSE_VOLUME_MAX_DB	(40 * 100)
#define SOF_SOUND_DOSE_GAIN_MIN_DB	(-100 * 100)	/* -100 to 0 dB */
#define SOF_SOUND_DOSE_GAIN_MAX_DB	(0 * 100)

struct sof_sound_dose {
	int16_t	mel_value;			/* Decibels x100, e.g. 85 dB is 8500 */
	int16_t dbfs_value;			/* Decibels x100 */
	int16_t current_sens_dbfs_dbspl;	/* Decibels x100 */
	int16_t current_volume_offset;		/* Decibels x100 */
	int16_t current_gain;			/* Decibels x100 */
	uint16_t reserved16;			/**< reserved for future use */
	uint32_t reserved32[4];			/**< reserved for future use */
} __attribute__((packed));

struct sound_dose_setup_config {
	int16_t sens_dbfs_dbspl;		/* Decibels x100 */
	int16_t reserved;
} __attribute__((packed));

struct sound_dose_volume_config {
	int16_t volume_offset;			/* Decibels x100 */
	int16_t reserved;
} __attribute__((packed));

struct sound_dose_gain_config {
	int16_t gain;				/* Decibels x100 */
	int16_t reserved;
} __attribute__((packed));

#endif /* __USER_SOUND_DOSE_H__ */
