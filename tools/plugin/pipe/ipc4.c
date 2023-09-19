// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * SOF pipeline in userspace.
 */

#define _GNU_SOURCE

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
#include <getopt.h>
#include <dlfcn.h>

#include <rtos/sof.h>
#include <rtos/task.h>
#include <sof/lib/notifier.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/topology.h>
#include <sof/lib/agent.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>

#include "common.h"
#include "pipe.h"

// TODO: take prefix from ALSA prefix
#define COMP_PREFIX	"./sof_ep/install/lib/libsof_"
#define COMP_SUFFIX	".so"
#define UUID_STR_SIZE	32

struct sof_pipe_module_library_map {
	int module_id;
	const char *name;
};

static const struct sof_pipe_module_library_map library_map[] = {
	{0x6, "libsof_volume.so"},
	{0x2, "libsof_mixer.so"},
	{0x3, "libsof_mixer.so"},
	/*FIXME: hack for now to set up ALSA and SHM components */
	{0x96, "libsof_mod_shm.so"}, /* host playback */
	{0x97, "libsof_mod_alsa.so"}, /* dai playback */
	{0x98, "libsof_mod_shm.so"}, /* host capture */
	{0x99, "libsof_mod_alsa.so"}, /* dai capture */
};

static int pipe_register_comp(struct sof_pipe *sp, uint16_t module_id)
{
	const struct sof_pipe_module_library_map *lib;
	int i;

	/* check if module already loaded */
	for (i = 0; i < sp->mod_idx; i++) {
		if (sp->module[i].module_id == module_id)
			return 0; /* module found and already loaded */
	}

	for (i = 0; i < ARRAY_SIZE(library_map); i++) {
		lib = &library_map[i];

		if (module_id == lib->module_id)
			break;
	}

	if (i == ARRAY_SIZE(library_map)) {
		fprintf(stderr, "module ID: %d not supported\n", module_id);
		return -ENOTSUP;
	}

	/* not loaded, so load module */
	sp->module[sp->mod_idx].handle = dlopen(lib->name, RTLD_NOW);
	if (!sp->module[sp->mod_idx].handle) {
		fprintf(stderr, "error: cant load module %s: %s\n",
			lib->name, dlerror());
		return -errno;
	}

	sp->mod_idx++;

	return 0;
}

#define iCS(x) ((x) & SOF_CMD_TYPE_MASK)
#define iGS(x) ((x) & SOF_GLB_TYPE_MASK)

static int pipe_sof_ipc_cmd_before(struct sof_pipe *sp, void *mailbox, size_t bytes)
{
	struct ipc4_message_request *in = mailbox;
	enum ipc4_message_target target = in->primary.r.msg_tgt;
	int ret = 0;

	switch (target) {
	case SOF_IPC4_MESSAGE_TARGET_MODULE_MSG:
	{
		uint32_t type = in->primary.r.type;

		switch (type) {
		case SOF_IPC4_MOD_INIT_INSTANCE:
			struct ipc4_module_init_instance *module_init =
				(struct ipc4_module_init_instance *)in;

			ret = pipe_register_comp(sp, module_init->primary.r.module_id);
			break;
		default:
			break;
		}
		break;
	}
	case SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG:
	{
		uint32_t type = in->primary.r.type;

		switch (type) {
		case SOF_IPC4_GLB_SET_PIPELINE_STATE:
		{
			struct ipc4_pipeline_set_state *state;
			struct ipc_comp_dev *ipc_pipe;
			struct ipc *ipc = ipc_get();
			unsigned int pipeline_id;

			state = (struct ipc4_pipeline_set_state *)in;
			pipeline_id = (unsigned int)state->primary.r.ppl_id;

			if (state->primary.r.ppl_state == SOF_IPC4_PIPELINE_STATE_PAUSED) {
				ipc_pipe = ipc_get_pipeline_by_id(ipc, pipeline_id);
				if (!ipc_pipe) {
					fprintf(stderr, "No pipeline with instance_id = %u",
						pipeline_id);
					return -EINVAL;
				}

				/* stop the pipeline thread */
				ret = pipe_thread_stop(sp, ipc_pipe->pipeline);
				if (ret < 0) {
					printf("error: can't start pipeline %d thread\n",
					       ipc_pipe->pipeline->comp_id);
					return ret;
				}
			}
			break;
		}
		default:
			break;
		}
		break;
	}
	default:
		fprintf(sp->log, "ipc: unknown command target %u size %ld",
			target, bytes);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int pipe_sof_ipc_cmd_after(struct sof_pipe *sp, void *mailbox, size_t bytes)
{
	struct ipc4_message_request *in = mailbox;
	enum ipc4_message_target target = in->primary.r.msg_tgt;
	int ret = 0;

	switch (target) {
	case SOF_IPC4_MESSAGE_TARGET_MODULE_MSG:
		break;
	case SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG:
	{
		uint32_t type = in->primary.r.type;

		switch (type) {
		case SOF_IPC4_GLB_CREATE_PIPELINE:
		{
			struct ipc4_pipeline_create *pipe_desc = (struct ipc4_pipeline_create *)in;
			struct ipc_comp_dev *ipc_pipe;
			struct ipc *ipc = ipc_get();
			unsigned int pipeline_id = (unsigned int)pipe_desc->primary.r.instance_id;

			ipc_pipe = ipc_get_pipeline_by_id(ipc, pipeline_id);
			if (!ipc_pipe) {
				fprintf(stderr, "No pipeline with instance_id = %u\n",
					pipeline_id);
				return -EINVAL;
			}

			/* create new pipeline thread */
			ret = pipe_thread_new(sp, ipc_pipe->pipeline);
			if (ret < 0) {
				printf("error: can't create pipeline %d thread\n",
				       ipc_pipe->pipeline->pipeline_id);
				return ret;
			}
			break;
		}
		case SOF_IPC4_GLB_SET_PIPELINE_STATE:
		{
			struct ipc4_pipeline_set_state *state;
			struct ipc_comp_dev *ipc_pipe;
			struct ipc *ipc = ipc_get();
			unsigned int pipeline_id;

			state = (struct ipc4_pipeline_set_state *)in;
			pipeline_id = (unsigned int)state->primary.r.ppl_id;

			if (state->primary.r.ppl_state == SOF_IPC4_PIPELINE_STATE_RUNNING) {
				ipc_pipe = ipc_get_pipeline_by_id(ipc, pipeline_id);
				if (!ipc_pipe) {
					fprintf(stderr, "No pipeline with instance_id = %u\n",
						pipeline_id);
					return -EINVAL;
				}

				/* start the pipeline thread */
				ret = pipe_thread_start(sp, ipc_pipe->pipeline);
				if (ret < 0) {
					printf("error: can't start pipeline %d thread\n",
					       ipc_pipe->pipeline->comp_id);
					return ret;
				}
			}
			break;
		}
		case SOF_IPC4_GLB_DELETE_PIPELINE:
		{
			struct ipc4_pipeline_create *pipe_desc = (struct ipc4_pipeline_create *)in;
			unsigned int pipeline_id = (unsigned int)pipe_desc->primary.r.instance_id;

			/* free pipeline thread */
			ret = pipe_thread_free(sp, pipeline_id);
			if (ret < 0) {
				printf("error: can't free pipeline %d thread\n",
				       pipeline_id);
				return ret;
			}

			break;
		}
		default:
			break;
		}
		break;
	}
	default:
		fprintf(sp->log, "ipc: unknown command target %u size %ld",
			target, bytes);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pipe_ipc_do(struct sof_pipe *sp, void *mailbox, size_t bytes)
{
	char mailbox_copy[IPC3_MAX_MSG_SIZE] = {0};
	int err = 0;

	/* preserve mailbox contents for local "after" config */
	memcpy(mailbox_copy, mailbox, bytes);

	/* some IPCs require pipe to perform actions before core */
	/* mailbox can be re-written here by local pipe if needed */
	err = pipe_sof_ipc_cmd_before(sp, mailbox, bytes);
	if (err < 0) {
		fprintf(sp->log, "error: local IPC processing failed\n");
		return err;
	}

	/* is the IPC local only or do we need send to infra ? */
	err = pipe_ipc_message(sp, mailbox, bytes);
	if (err < 0) {
		fprintf(sp->log, "error: infra IPC processing failed\n");
		return err;
	}

	/* some IPCs require pipe to perform actions before core */
	err = pipe_sof_ipc_cmd_after(sp, mailbox_copy, bytes);
	if (err < 0) {
		fprintf(sp->log, "error: local IPC processing failed\n");
		return err;
	}

	return err;
}

int pipe_ipc_process(struct sof_pipe *sp, struct plug_mq_desc *tx_mq, struct plug_mq_desc *rx_mq)
{
	ssize_t ipc_size;
	char mailbox[IPC3_MAX_MSG_SIZE] = {0};
	int err;
	struct timespec ts;

	/* IPC thread should not preempt processing thread */
	err = pipe_set_ipc_lowpri(sp);
	if (err < 0)
		fprintf(sp->log, "error: cant set PCM IPC thread to low priority");

	/* create the IPC message queue */
	err = plug_mq_create(tx_mq);
	if (err < 0) {
		fprintf(sp->log, "error: can't create TX IPC message queue : %s\n",
			strerror(errno));
		return -errno;
	}

	/* create the IPC message queue */
	err = plug_mq_create(rx_mq);
	if (err < 0) {
		fprintf(sp->log, "error: can't create PCM IPC message queue : %s\n",
			strerror(errno));
		return -errno;
	}

	/* let main() know we are ready */
	fprintf(sp->log, "sof-pipe: IPC TX %s thread ready\n", tx_mq->queue_name);
	fprintf(sp->log, "sof-pipe: IPC RX %s thread ready\n", rx_mq->queue_name);

	/* main PCM IPC handling loop */
	while (1) {
		memset(mailbox, 0, IPC3_MAX_MSG_SIZE);

		/* is client dead ? */
		if (sp->glb->state == SOF_PLUGIN_STATE_DEAD) {
			fprintf(sp->log, "sof-pipe: IPC %s client complete\n", tx_mq->queue_name);
			break;
		}

		ipc_size = mq_receive(tx_mq->mq, mailbox, IPC3_MAX_MSG_SIZE, NULL);
		if (ipc_size < 0) {
			fprintf(sp->log, "error: can't read PCM IPC message queue %s : %s\n",
				tx_mq->queue_name, strerror(errno));
			break;
		}

		/* TODO: properly validate message and continue if garbage */
		if (*((uint32_t *)mailbox) == 0) {
			fprintf(sp->log, "sof-pipe: IPC %s garbage read\n", tx_mq->queue_name);
			ts.tv_sec = 0;
			ts.tv_nsec = 20 * 1000 * 1000; /* 20 ms */
			nanosleep(&ts, NULL);
			continue;
		}

		/* do the message work */
		//data_dump(mailbox, IPC3_MAX_MSG_SIZE);

		err = pipe_ipc_do(sp, mailbox, ipc_size);
		if (err < 0)
			fprintf(sp->log, "error: local IPC processing failed\n");

		/* now return message completion status found in mailbox */
		err = mq_send(rx_mq->mq, mailbox, IPC3_MAX_MSG_SIZE, 0);
		if (err < 0) {
			fprintf(sp->log, "error: can't send PCM IPC message queue %s : %s\n",
				rx_mq->queue_name, strerror(errno));
			break;
		}
	}

	fprintf(sp->log, "***sof-pipe: IPC %s thread finished !!\n", tx_mq->queue_name);
	return 0;
}

int plug_mq_cmd(struct plug_mq_desc *ipc, void *msg, size_t len, void *reply, size_t rlen)
{
	struct timespec ts;
	ssize_t ipc_size;
	char mailbox[IPC3_MAX_MSG_SIZE];
	int err;

	if (len > IPC3_MAX_MSG_SIZE) {
		SNDERR("ipc: message too big %d\n", len);
		return -EINVAL;
	}
	memset(mailbox, 0, IPC3_MAX_MSG_SIZE);
	memcpy(mailbox, msg, len);

	/* wait for sof-pipe reader to consume data or timeout */
	err = clock_gettime(CLOCK_REALTIME, &ts);
	if (err == -1) {
		SNDERR("ipc: cant get time: %s", strerror(errno));
		return -errno;
	}

	/* IPCs should be read under 10ms */
	plug_timespec_add_ms(&ts, 10);

	/* now return message completion status */
	err = mq_timedsend(ipc->mq, mailbox, IPC3_MAX_MSG_SIZE, 0, &ts);
	if (err < 0) {
		SNDERR("error: can't send IPC message queue %s : %s\n",
		       ipc->queue_name, strerror(errno));
		return -errno;
	}

	/* wait for sof-pipe reader to consume data or timeout */
	err = clock_gettime(CLOCK_REALTIME, &ts);
	if (err == -1) {
		SNDERR("ipc: cant get time: %s", strerror(errno));
		return -errno;
	}

	/* IPCs should be processed under 20ms */
	plug_timespec_add_ms(&ts, 20);

	ipc_size = mq_timedreceive(ipc->mq, mailbox, IPC3_MAX_MSG_SIZE, NULL, &ts);
	if (ipc_size < 0) {
		SNDERR("error: can't read IPC message queue %s : %s\n",
		       ipc->queue_name, strerror(errno));
		return -errno;
	}

	/* do the message work */
	//printf("cmd got IPC %ld reply bytes\n", ipc_size);
	if (rlen && reply)
		memcpy(reply, mailbox, rlen);

	return 0;
}
