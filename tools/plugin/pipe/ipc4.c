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
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
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
	{0x95, "libsof_ns.so"},
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
		{
			struct ipc4_module_init_instance *module_init =
				(struct ipc4_module_init_instance *)in;

			ret = pipe_register_comp(sp, module_init->primary.r.module_id);
			break;
		}
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

static void *handle_ipc_client(void *data)
{
	struct sof_pipe *sp = data;
	struct plug_socket_desc *ipc_socket = &sp->ipc_socket;
	struct timespec ts;
	int client_id = sp->new_client_id;
	int clientfd = sp->client_sock_ids[client_id];
	char mailbox[IPC3_MAX_MSG_SIZE] = {0};
	ssize_t ipc_size;
	int err;

	while (1) {
		memset(mailbox, 0, IPC3_MAX_MSG_SIZE);

		ipc_size = recv(clientfd, mailbox, IPC3_MAX_MSG_SIZE, 0);
		if (ipc_size < 0) {
			fprintf(sp->log, "error: can't read IPC socket %s : %s\n",
				ipc_socket->path, strerror(errno));
			break;
		}

		/* connection lost */
		if (ipc_size == 0) {
			close(clientfd);
			break;
		}

		/* TODO: properly validate message and continue if garbage */
		if (*((uint32_t *)mailbox) == 0) {
			fprintf(sp->log, "sof-pipe: IPC %s garbage read\n", ipc_socket->path);
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
		err = send(clientfd, mailbox, IPC3_MAX_MSG_SIZE, 0);
		if (err < 0) {
			close(clientfd);
			break;
		}
	}

	close(clientfd);
	sp->client_sock_ids[client_id] = 0;
	return NULL;
}

int pipe_ipc_process(struct sof_pipe *sp, struct plug_socket_desc *ipc_socket)
{
	pthread_t thread_id;
	int err;

	/* IPC thread should not preempt processing thread */
	err = pipe_set_ipc_lowpri(sp);
	if (err < 0)
		fprintf(sp->log, "error: cant set PCM IPC thread to low priority");

	/* create the IPC socket */
	err = plug_socket_create(ipc_socket);
	if (err < 0) {
		fprintf(sp->log, "error: can't create TX IPC socket : %s\n",
			strerror(errno));
		return -errno;
	}

	/* let main() know we are ready */
	fprintf(sp->log, "sof-pipe: IPC %s socket ready\n", ipc_socket->path);

	/* main PCM IPC handling loop */
	while (1) {
		int clientfd, i;

		/* Accept a connection from a client */
		clientfd = accept(ipc_socket->socket_fd, NULL, NULL);
		if (clientfd == -1) {
			fprintf(sp->log, "IPC %s socket accept failed\n", ipc_socket->path);
			return -errno;
		}

		for (i = 0; i < MAX_IPC_CLIENTS; i++) {
			if (!sp->client_sock_ids[i]) {
				sp->client_sock_ids[i] = clientfd;
				sp->new_client_id = i;
				break;
			}
		}

		/* create a new thread for each new IPC client */
		if (pthread_create(&thread_id, NULL, handle_ipc_client, sp) != 0) {
			fprintf(sp->log, "Client IPC thread creation failed");
			close(clientfd);
			sp->client_sock_ids[i] = 0;
			continue;
		}

		fprintf(sp->log, "Client IPC thread created client id %d\n", clientfd);
		/* Detach the thread */
		pthread_detach(thread_id);
	}

	close(ipc_socket->socket_fd);

	fprintf(sp->log, "***sof-pipe: IPC %s thread finished !!\n", ipc_socket->path);
	return 0;
}
