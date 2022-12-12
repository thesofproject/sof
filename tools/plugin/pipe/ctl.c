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
		struct snd_soc_tplg_mixer_control *tplg_mixer =
			(struct snd_soc_tplg_mixer_control *)tplg_ctl;
		glb->size += sizeof(struct plug_shm_ctl);
		ctl = &glb->ctl[glb->num_ctls++];
		ctl->comp_id = comp->id;
		ctl->mixer_ctl = *tplg_mixer;
		break;
	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
		struct snd_soc_tplg_enum_control *tplg_enum =
			(struct snd_soc_tplg_enum_control *)tplg_ctl;
		glb->size += sizeof(struct plug_shm_ctl);
		ctl = &glb->ctl[glb->num_ctls++];
		ctl->comp_id = comp->id;
		ctl->enum_ctl = *tplg_enum;
		break;
	case SND_SOC_TPLG_CTL_BYTES:
		struct snd_soc_tplg_bytes_control *tplg_bytes =
			(struct snd_soc_tplg_bytes_control *)tplg_ctl;
		glb->size += sizeof(struct plug_shm_ctl);
		ctl = &glb->ctl[glb->num_ctls++];
		ctl->comp_id = comp->id;
		ctl->bytes_ctl = *tplg_bytes;
		break;
	case SND_SOC_TPLG_CTL_RANGE:
	case SND_SOC_TPLG_CTL_STROBE:
	default:
		fprintf(sp->log, "error: invalid ctl type %d\n",
			tplg_ctl->type);
		return -EINVAL;
	}

	return 0;
}

#if 0
static void *pipe_ipc_ctl_thread(void *arg)
{
	struct sof_pipe *sp = arg;
	ssize_t ipc_size;
	char mailbox[384] = {0};
	int err;

	/* IPC thread should not preempt processing thread */
	err = pipe_set_ipc_lowpri(sp);
	if (err < 0)
		fprintf(sp->log, "error: cant set CTL IPC thread to low priority");

	/* let main() know we are ready */
	fprintf(sp->log, "sof-pipe: CTL IPC thread ready\n");

	/* main CTL IPC handling loop */
	while (1) {
		ipc_size = mq_receive(sp->ctl_ipc.mq, mailbox, IPC3_MAX_MSG_SIZE, NULL);
		if (err < 0) {
			fprintf(sp->log, "error: can't read CTL IPC message queue %s : %s\n",
				sp->ctl_ipc.queue_name, strerror(errno));
			break;
		}

		/* do the message work */
		printf("IPC-CTL: got %ld bytes\n", ipc_size);
		if (sp->dead)
			break;
		pipe_ipc_message(sp, mailbox);

		/* now return message completion status */
		err = mq_send(sp->ctl_ipc.mq, mailbox, IPC3_MAX_MSG_SIZE, 0);
		if (err < 0) {
			fprintf(sp->log, "error: can't send CTL IPC message queue %s : %s\n",
				sp->ctl_ipc.queue_name, strerror(errno));
			break;
		}
	}

	fprintf(sp->log, "CTL IPC thread finished !!\n");
	return NULL;
}

/* invoked on */
int pipe_ctl_new(struct sof_pipe *sp, int pri, int core)
{
	int ret;

	/* start CTL IPC thread */
	ret = pthread_create(&sp->ipc_ctl_thread, NULL, &pipe_ipc_ctl_thread, sp);
	if (ret < 0) {
		fprintf(sp->log, "failed to create CTL IPC thread: %s\n", strerror(errno));
		return -errno;
	}

	return ret;
}

int pipe_ctl_free(struct sof_pipe *sp)
{
	int ret;

	/* start CTL IPC thread */
	ret = pthread_create(&sp->ipc_ctl_thread, NULL, &pipe_ipc_ctl_thread, sp);
	if (ret < 0) {
		fprintf(sp->log, "failed to create CTL IPC thread: %s\n", strerror(errno));
		return -errno;
	}

	return ret;
}
#endif
