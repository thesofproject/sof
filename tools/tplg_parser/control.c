// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

/* Topology parser */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <ipc/topology.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/topology.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

int tplg_get_single_control(struct tplg_context *ctx,
			    struct snd_soc_tplg_ctl_hdr **ctl,
			    struct snd_soc_tplg_private **priv_data)
{
	struct snd_soc_tplg_ctl_hdr *ctl_hdr;
	struct snd_soc_tplg_mixer_control *mixer_ctl = NULL;
	struct snd_soc_tplg_enum_control *enum_ctl = NULL;
	struct snd_soc_tplg_bytes_control *bytes_ctl = NULL;

	/* These are set if success */
	*ctl = NULL;
	*priv_data = NULL;

	ctl_hdr = tplg_get(ctx);

	/* load control based on type */
	switch (ctl_hdr->ops.info) {
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_CTL_STROBE:
	case SND_SOC_TPLG_CTL_VOLSW_SX:
	case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
	case SND_SOC_TPLG_CTL_RANGE:
	case SND_SOC_TPLG_DAPM_CTL_VOLSW:
		/* load mixer type control */
		mixer_ctl = (struct snd_soc_tplg_mixer_control *)ctl_hdr;
		if (priv_data)
			*priv_data = &mixer_ctl->priv;
		/* ctl is after private data */
		*ctl = tplg_get_object_priv(ctx, mixer_ctl, mixer_ctl->priv.size);
		break;

	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
		/* load enum type control */
		enum_ctl = (struct snd_soc_tplg_enum_control *)ctl_hdr;
		if (priv_data)
			*priv_data = &enum_ctl->priv;
		/* ctl is after private data */
		*ctl = tplg_get_object_priv(ctx, enum_ctl, enum_ctl->priv.size);
		break;
	case SND_SOC_TPLG_CTL_BYTES:
		/* load bytes type control */
		bytes_ctl = (struct snd_soc_tplg_bytes_control *)ctl_hdr;
		if (priv_data)
			*priv_data = &bytes_ctl->priv;
		/* ctl is after private data */
		*ctl = tplg_get_object_priv(ctx, bytes_ctl, bytes_ctl->priv.size);
		break;

	default:
		printf("info: control type %d not supported\n",
		       ctl_hdr->ops.info);
		return -EINVAL;
	}

	return 0;
}

/* load dapm widget kcontrols
 * we don't use controls in the fuzzer atm.
 * so just skip to the next dapm widget
 */
int tplg_create_controls(struct tplg_context *ctx, int num_kcontrols,
			 struct snd_soc_tplg_ctl_hdr *rctl,
			 size_t max_ctl_size, void *object)
{
	struct snd_soc_tplg_ctl_hdr *ctl_hdr = NULL;
	struct snd_soc_tplg_mixer_control *mixer_ctl;
	struct snd_soc_tplg_enum_control *enum_ctl;
	struct snd_soc_tplg_bytes_control *bytes_ctl;
	int num_mixers = 0;
	int num_enums = 0;
	int num_byte_controls = 0;
	int index;
	int j, ret = 0;

	for (j = 0; j < num_kcontrols; j++) {

		ctl_hdr = tplg_get(ctx);

		/* load control based on type */
		switch (ctl_hdr->ops.info) {
		case SND_SOC_TPLG_CTL_VOLSW:
		case SND_SOC_TPLG_CTL_STROBE:
		case SND_SOC_TPLG_CTL_VOLSW_SX:
		case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
		case SND_SOC_TPLG_CTL_RANGE:
		case SND_SOC_TPLG_DAPM_CTL_VOLSW:
			index = num_mixers++;
			/* load mixer type control */
			mixer_ctl = (struct snd_soc_tplg_mixer_control *)ctl_hdr;
			/* ctl is after private data */
			tplg_get_object_priv(ctx, mixer_ctl, mixer_ctl->priv.size);
			break;

		case SND_SOC_TPLG_CTL_ENUM:
		case SND_SOC_TPLG_CTL_ENUM_VALUE:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
			index = num_enums++;
			/* load enum_ctl type control */
			enum_ctl = (struct snd_soc_tplg_enum_control *)ctl_hdr;
			/* ctl is after private data */
			tplg_get_object_priv(ctx, enum_ctl, enum_ctl->priv.size);
			break;

		case SND_SOC_TPLG_CTL_BYTES:
			index = num_byte_controls++;
			/* load bytes_ctl type control */
			bytes_ctl = (struct snd_soc_tplg_bytes_control *)ctl_hdr;
			/* ctl is after private data */
			tplg_get_object_priv(ctx, bytes_ctl, bytes_ctl->priv.size);
			break;
		default:
			printf("info: control type %d not supported\n",
			       ctl_hdr->ops.info);
			return -EINVAL;
		}

		if (ctx->ctl_cb && object)
			ctx->ctl_cb(ctl_hdr, object, ctx->ctl_arg, index);
	}

	if (rctl && ctl_hdr) {
		/* make sure the CTL will fit if we need to copy it for others */
		if (ctl_hdr->size > max_ctl_size) {
			fprintf(stderr, "error: failed control control copy\n");
			ret = -EINVAL;
			goto err;
		}
		memcpy(rctl, ctl_hdr, ctl_hdr->size);
	}
err:
	return ret;
}
