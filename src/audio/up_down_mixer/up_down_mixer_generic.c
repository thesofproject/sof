// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022-2025 Intel Corporation.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@intel.com>
//         Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "up_down_mixer.h"

#if SOF_USE_HIFI(NONE, UP_DOWN_MIXER)

void upmix32bit_1_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void upmix16bit_1_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void upmix32bit_2_0_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void upmix16bit_2_0_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void upmix32bit_2_0_to_7_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void shiftcopy32bit_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void shiftcopy32bit_stereo(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_2_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_3_0(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_3_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		  const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_4_0(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_5_0_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_7_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix16bit_stereo(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void shiftcopy16bit_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void shiftcopy16bit_stereo(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix16bit(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		  const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix16bit_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix16bit_4ch_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_stereo(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_3_1_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_4_0_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_quatro_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			      const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_5_1_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_7_1_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void downmix32bit_7_1_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			     const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void upmix32bit_4_0_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

void upmix32bit_quatro_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			      const uint32_t in_size, uint8_t * const out_data)
{
	sof_panic(0);
}

#endif /* #if SOF_USE_HIFI(NONE, UP_DOWN_MIXER) */
