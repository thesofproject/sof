// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@intel.com>
// Author: Adrian Bonislawski <adrian.bonislawski@intel.com>

#ifndef __SOF_AUDIO_UP_DOWN_MIXER_H__
#define __SOF_AUDIO_UP_DOWN_MIXER_H__

#include <../include/ipc4/up_down_mixer.h>
#include <stddef.h>
#include <stdint.h>
#include <../include/util_macro.h>
/** This type is introduced for better readability. */
typedef const int32_t *downmix_coefficients;

/** forward declaration */
struct up_down_mixer_data;



/** Function's pointer to up/down-mix routines. */
typedef void (*up_down_mixer_routine)(struct up_down_mixer_data *cd,
				      const uint8_t * const in_data,
				      const uint32_t in_size,
				      uint8_t * const out_data);

/**
 * Simple 'macro-style' method to create proper channel map from a valid
 * channel configuration.
 */
static inline channel_map create_channel_map(enum ipc4_channel_config channel_config)
{
	switch (channel_config) {
	case IPC4_CHANNEL_CONFIG_MONO:
		return (0xFFFFFFF0 | CHANNEL_CENTER);
	case IPC4_CHANNEL_CONFIG_STEREO:
		return (0xFFFFFF00 | CHANNEL_LEFT | (CHANNEL_RIGHT << 4));
	case IPC4_CHANNEL_CONFIG_2_POINT_1:
		return (0xFFFFF000 | CHANNEL_LEFT | (CHANNEL_RIGHT << 4) | (CHANNEL_LFE << 8));
	case IPC4_CHANNEL_CONFIG_3_POINT_0:
		return (0xFFFFF000 | CHANNEL_LEFT | (CHANNEL_CENTER << 4) | (CHANNEL_RIGHT << 8));
	case IPC4_CHANNEL_CONFIG_3_POINT_1:
		return (0xFFFF0000 | CHANNEL_LEFT | (CHANNEL_CENTER << 4) | (CHANNEL_RIGHT << 8)
				   | (CHANNEL_LFE << 12));
	case IPC4_CHANNEL_CONFIG_QUATRO:
		return (0xFFFF0000 | CHANNEL_LEFT | (CHANNEL_RIGHT << 4)
				   | (CHANNEL_LEFT_SURROUND << 8) | (CHANNEL_RIGHT_SURROUND << 12));
	case IPC4_CHANNEL_CONFIG_4_POINT_0:
		return (0xFFFF0000 | CHANNEL_LEFT | (CHANNEL_CENTER << 4) | (CHANNEL_RIGHT << 8)
				   | (CHANNEL_CENTER_SURROUND << 12));
	case IPC4_CHANNEL_CONFIG_5_POINT_0:
		return (0xFFF00000 | CHANNEL_LEFT | (CHANNEL_CENTER << 4) | (CHANNEL_RIGHT << 8)
				   | (CHANNEL_LEFT_SURROUND << 12)
				   | (CHANNEL_RIGHT_SURROUND << 16));
	case IPC4_CHANNEL_CONFIG_5_POINT_1:
		return (0xFF000000 | CHANNEL_LEFT
				   | (CHANNEL_CENTER << 4)
				   | (CHANNEL_RIGHT << 8)
				   | (CHANNEL_LEFT_SURROUND << 12)
				   | (CHANNEL_RIGHT_SURROUND << 16)
				   | (CHANNEL_LFE << 20));
	case IPC4_CHANNEL_CONFIG_7_POINT_1:
		return (CHANNEL_LEFT | (CHANNEL_CENTER << 4)
				     | (CHANNEL_RIGHT << 8)
				     | (CHANNEL_LEFT_SURROUND << 12)
				     | (CHANNEL_RIGHT_SURROUND << 16)
				     | (CHANNEL_LFE << 20)
				     | (CHANNEL_LEFT_SIDE << 24)
				     | (CHANNEL_RIGHT_SIDE << 28));
	case IPC4_CHANNEL_CONFIG_DUAL_MONO:
		return (0xFFFFFF00 | CHANNEL_LEFT | (CHANNEL_LEFT << 4));
	case IPC4_CHANNEL_CONFIG_I2S_DUAL_STEREO_0:
		return (0xFFFFFF00 | CHANNEL_LEFT | (CHANNEL_RIGHT << 4));
	case IPC4_CHANNEL_CONFIG_I2S_DUAL_STEREO_1:
		return (0xFFFF00FF | (CHANNEL_LEFT << 8) | (CHANNEL_RIGHT << 12));
	default:
		return 0xFFFFFFFF;
	}
}

static inline uint8_t get_channel_location(const channel_map map,
					   const enum ipc4_channel_index channel)
{
	uint8_t offset = 0xF;
	uint8_t i;

	/* Search through all 4 bits of each byte in the integer for the channel. */
	for (i = 0; i < 8; i++) {
		if (((map >> (i * 4)) & 0xF) == (uint8_t)channel) {
			offset = i;
			break;
		}
	}

	return offset;
}

static inline enum ipc4_channel_index get_channel_index(const channel_map map,
							const uint8_t location)
{
	return (enum ipc4_channel_index)((map >> (location * 4)) & 0xF);
}

/**
 * \brief up_down_mixer component private data.
 */
struct up_down_mixer_data {
	/** Number of channels in the input buffer. */
	size_t in_channel_no;

	/** Channel map in the input buffer. */
	channel_map in_channel_map;

	/** Channel configuration in the input buffer. */
	enum ipc4_channel_config in_channel_config;
	channel_map out_channel_map;

	/** Function pointer to up/down-mix routine. */
	up_down_mixer_routine mix_routine;

	/** Downmix coefficients. */
	downmix_coefficients downmix_coefficients;

	struct ipc4_audio_format out_fmt[IPC4_UP_DOWN_MIXER_MODULE_OUTPUT_PINS_COUNT];

	const int32_t k_lo_ro_downmix32bit[UP_DOWN_MIX_COEFFS_LENGTH];
	const int32_t k_scaled_lo_ro_downmix32bit[UP_DOWN_MIX_COEFFS_LENGTH];
	const int32_t k_half_scaled_lo_ro_downmix32bit[UP_DOWN_MIX_COEFFS_LENGTH];
	const int32_t k_quatro_mono_scaled_lo_ro_downmix32bit[UP_DOWN_MIX_COEFFS_LENGTH];
	const int32_t k_lo_ro_downmix16bit[UP_DOWN_MIX_COEFFS_LENGTH];
	const int32_t k_scaled_lo_ro_downmix16bit[UP_DOWN_MIX_COEFFS_LENGTH];
	const int32_t k_half_scaled_lo_ro_downmix16bit[UP_DOWN_MIX_COEFFS_LENGTH];
	const int32_t k_quatro_mono_scaled_lo_ro_downmix16bit[UP_DOWN_MIX_COEFFS_LENGTH];

	/** In/out internal buffers */
	int32_t *buf_in;
	int32_t *buf_out;
};

/**
 * \brief 32 bit upmixer (mono -> 5_1).
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void upmix32bit_1_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 16 bit upmixer (mono -> 5_1).
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void upmix16bit_1_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit upmixer (2_0 -> 5_1).
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void upmix32bit_2_0_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 16 bit upmixer (2_0 -> 5_1).
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void upmix16bit_2_0_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit upmixer (2_0 -> 7_1).
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void upmix32bit_2_0_to_7_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit shift copier for mono streams.
 *		  Copy the 32 MSB input mono stream (left, right) to 32 MSB stereo one.
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void shiftcopy32bit_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit shift copier for stereo.
 *		  Copy the 32 MSB input streo stream (left, right) to 32 MSB one.
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void shiftcopy32bit_stereo(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 24 bit downmixer specialized for the 2.1.
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_2_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 24 bit downmixer specialized for the 3.0.
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_3_0(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 24 bit downmixer specialized for the 3.1.
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_3_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 24 bit downmixer. This function is highly power consuming
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		  const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 24 bit downmixer specialized for the 4.0.
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_4_0(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit downmixer specialized for the 5.0 to mono downmixing.
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_5_0_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit downmixer specialized for the 5.1 to mono downmixing.
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit downmixer specialized for the 7.1 to mono downmixing.
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_7_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 16 bit shift copier for mono.
 *		  Copy the 16 bit input mono stream (left, right) to 32 MSB stereo one.
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void shiftcopy16bit_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 16 bit shift copier for stereo.
 *		  Copy the 16 bit input streo stream (left, right) to 32 MSB one.
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void shiftcopy16bit_stereo(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 16 bit downmixer. This function is highly power consuming
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix16bit(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		  const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 16 bit downmixer specialized for the 5.1.
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix16bit_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 16 bit downmixer 4 channels to mono. This function is highly power consuming
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix16bit_4ch_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 24 bit downmixer specialized for the 2.0.
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_stereo(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 16 bit downmix from stereo to mono.
 * \note needs to be optimized!!!
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix16bit_stereo(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit downmixer specialized for the 3.1 to mono downmixing.
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_3_1_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit downmixer specialized for the 4.0 to mono downmixing.
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_4_0_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit downmixer specialized for the Quatro to mono downmixing.
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_quatro_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			      const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit downmixer specialized for the 5.1 to mono downmixing.
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_5_1_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit downmixer specialized for the 7.1 to mono downmixing.
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_7_1_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit downmixer specialized for the 7.1 to 5_1 downmixing.
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void downmix32bit_7_1_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			     const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit downmixer specialized for the 4.0 to 5_1
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void upmix32bit_4_0_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data);

/**
 * \brief 32 bit downmixer specialized for the Quatro to 5_1
 * \note implementation is based on Downmix32bit
 *
 * \param[in]   cd                      Component private data.
 * \param[in]   in_data                 Input buffer.
 * \param[in]   in_size                 Input buffer size.
 * \param[out]  out_data                Output buffer.
 */
void upmix32bit_quatro_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			      const uint32_t in_size, uint8_t * const out_data);

#endif /* __SOF_AUDIO_UP_DOWN_MIXER_H__ */
