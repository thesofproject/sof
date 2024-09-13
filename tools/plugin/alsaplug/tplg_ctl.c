// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//

/* SOF topology kcontrols */

#include <stdio.h>
#include <sys/poll.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <math.h>

#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>

#include <rtos/sof.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <ipc/stream.h>
#include <tplg_parser/topology.h>

#include "plugin.h"
#include "common.h"

#define SOF_IPC4_VOL_ZERO_DB	0x7fffffff
#define VOLUME_FWL      16
/*
 * Constants used in the computation of linear volume gain
 * from dB gain 20th root of 10 in Q1.16 fixed-point notation
 */
#define VOL_TWENTIETH_ROOT_OF_TEN	73533
/* 40th root of 10 in Q1.16 fixed-point notation*/
#define VOL_FORTIETH_ROOT_OF_TEN	69419

/* 0.5 dB step value in topology TLV */
#define VOL_HALF_DB_STEP	50

/*
 * Function to truncate an unsigned 64-bit number
 * by x bits and return 32-bit unsigned number. This
 * function also takes care of rounding while truncating
 */
static uint32_t vol_shift_64(uint64_t i, uint32_t x)
{
	if (x == 0)
		return (uint32_t)i;

	/* do not truncate more than 32 bits */
	if (x > 32)
		x = 32;

	return (uint32_t)(((i >> (x - 1)) + 1) >> 1);
}

/*
 * Function to compute a ** exp where,
 * a is a fractional number represented by a fixed-point integer with a fractional word length
 * of "fwl"
 * exp is an integer
 * fwl is the fractional word length
 * Return value is a fractional number represented by a fixed-point integer with a fractional
 * word length of "fwl"
 */
static uint32_t vol_pow32(uint32_t a, int exp, uint32_t fwl)
{
	int i, iter;
	uint32_t power = 1 << fwl;
	unsigned long long numerator;

	/* if exponent is 0, return 1 */
	if (exp == 0)
		return power;

	/* determine the number of iterations based on the exponent */
	if (exp < 0)
		iter = exp * -1;
	else
		iter = exp;

	/* multiply a "iter" times to compute power */
	for (i = 0; i < iter; i++) {
		/*
		 * Product of 2 Qx.fwl fixed-point numbers yields a Q2*x.2*fwl
		 * Truncate product back to fwl fractional bits with rounding
		 */
		power = vol_shift_64((uint64_t)power * a, fwl);
	}

	if (exp > 0) {
		/* if exp is positive, return the result */
		return power;
	}

	/* if exp is negative, return the multiplicative inverse */
	numerator = (uint64_t)1 << (fwl << 1);
	numerator /= power;

	return (uint32_t)numerator;
}

/*
 * Function to calculate volume gain from TLV data.
 * This function can only handle gain steps that are multiples of 0.5 dB
 */
static uint32_t vol_compute_gain(uint32_t value, struct snd_soc_tplg_tlv_dbscale *scale)
{
	int dB_gain;
	uint32_t linear_gain;
	int f_step;

	/* mute volume */
	if (value == 0 && scale->mute)
		return 0;

	/* compute dB gain from tlv. tlv_step in topology is multiplied by 100 */
	dB_gain = (int)scale->min / 100 + (value * scale->step) / 100;

	/* compute linear gain represented by fixed-point int with VOLUME_FWL fractional bits */
	linear_gain = vol_pow32(VOL_TWENTIETH_ROOT_OF_TEN, dB_gain, VOLUME_FWL);

	/* extract the fractional part of volume step */
	f_step = scale->step - (scale->step / 100);

	/* if volume step is an odd multiple of 0.5 dB */
	if (f_step == VOL_HALF_DB_STEP && (value & 1))
		linear_gain = vol_shift_64((uint64_t)linear_gain * VOL_FORTIETH_ROOT_OF_TEN,
					   VOLUME_FWL);

	return linear_gain;
}

/* helper function to add new kcontrols to the list of kcontrols in the global context */
int plug_kcontrol_cb_new(struct snd_soc_tplg_ctl_hdr *tplg_ctl, void *_comp, void *arg, int index)
{
	struct tplg_comp_info *comp_info = _comp;
	snd_sof_plug_t *plug = arg;
	struct plug_shm_glb_state *glb = plug->glb_ctx.addr;
	struct plug_shm_ctl *ctl;

	if (glb->num_ctls >= MAX_CTLS) {
		SNDERR("Failed to add a new control. Too many controls already\n");
		return -EINVAL;
	}

	switch (tplg_ctl->ops.info) {
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_CTL_VOLSW_SX:
	case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
	{
		struct snd_soc_tplg_mixer_control *tplg_mixer =
			(struct snd_soc_tplg_mixer_control *)tplg_ctl;
		struct snd_soc_tplg_ctl_tlv *tlv;
		struct snd_soc_tplg_tlv_dbscale *scale;
		int i;

		glb->size += sizeof(struct plug_shm_ctl);
		ctl = &glb->ctl[glb->num_ctls++];
		ctl->module_id = comp_info->module_id;
		ctl->instance_id = comp_info->instance_id;
		ctl->mixer_ctl = *tplg_mixer;
		ctl->index = index;
		tlv = &tplg_ctl->tlv;
		scale = &tlv->scale;

		/* populate the volume table */
		for (i = 0; i < tplg_mixer->max + 1 ; i++) {
			uint32_t val = vol_compute_gain(i, scale);

			/* Can be over Q1.31, need to saturate */
			uint64_t q31val = ((uint64_t)val) << 15;

			ctl->volume_table[i] = q31val > SOF_IPC4_VOL_ZERO_DB ?
							SOF_IPC4_VOL_ZERO_DB : q31val;
		}
		break;
	}
	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
	{
		struct snd_soc_tplg_enum_control *tplg_enum =
			(struct snd_soc_tplg_enum_control *)tplg_ctl;

		glb->size += sizeof(struct plug_shm_ctl);
		ctl = &glb->ctl[glb->num_ctls++];
		ctl->module_id = comp_info->module_id;
		ctl->instance_id = comp_info->instance_id;
		ctl->enum_ctl = *tplg_enum;
		ctl->index = index;
		break;
	}
	case SND_SOC_TPLG_CTL_BYTES:
		break;
	case SND_SOC_TPLG_CTL_RANGE:
	case SND_SOC_TPLG_CTL_STROBE:
	default:
		SNDERR("Invalid ctl type %d\n", tplg_ctl->type);
		return -EINVAL;
	}

	return 0;
}
