// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * SOF pipeline in userspace.
 */

#include <stdio.h>
#include <sys/poll.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <limits.h>

#include "common.h"
#include "pipe.h"

int pipe_kcontrol_cb_new(struct snd_soc_tplg_ctl_hdr *tplg_ctl,
			 void *_comp, void *arg)
{
	struct sof_pipe *sp = arg;
	struct sof_ipc_comp *comp = _comp;
	struct plug_shm_glb_state *glb = sp->glb;
	struct plug_shm_ctl *ctl;

	if (glb->num_ctls >= MAX_CTLS) {
		fprintf(sp->log, "error: too many ctls\n");
		return -EINVAL;
	}

	switch (tplg_ctl->type) {
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_CTL_VOLSW_SX:
	case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
	{
		struct snd_soc_tplg_mixer_control *tplg_mixer =
			(struct snd_soc_tplg_mixer_control *)tplg_ctl;

		glb->size += sizeof(struct plug_shm_ctl);
		ctl = &glb->ctl[glb->num_ctls++];
		ctl->comp_id = comp->id;
		ctl->mixer_ctl = *tplg_mixer;
		break;
	}
	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
	{
		struct snd_soc_tplg_enum_control *tplg_enum =
			(struct snd_soc_tplg_enum_control *)tplg_ctl;

		glb->size += sizeof(struct plug_shm_ctl);
		ctl = &glb->ctl[glb->num_ctls++];
		ctl->comp_id = comp->id;
		ctl->enum_ctl = *tplg_enum;
		break;
	}
	case SND_SOC_TPLG_CTL_BYTES:
	{
		struct snd_soc_tplg_bytes_control *tplg_bytes =
			(struct snd_soc_tplg_bytes_control *)tplg_ctl;

		glb->size += sizeof(struct plug_shm_ctl);
		ctl = &glb->ctl[glb->num_ctls++];
		ctl->comp_id = comp->id;
		ctl->bytes_ctl = *tplg_bytes;
		break;
	}
	case SND_SOC_TPLG_CTL_RANGE:
	case SND_SOC_TPLG_CTL_STROBE:
	default:
		fprintf(sp->log, "error: invalid ctl type %d\n",
			tplg_ctl->type);
		return -EINVAL;
	}

	return 0;
}
