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
#define COMP_PREFIX	"/usr/lib/x86_64-linux-gnu/alsa-lib/libsof-"
#define COMP_SUFFIX	".so"
#define UUID_STR_SIZE	32

static void pipe_comp_uuid(char *name, size_t size, struct sof_ipc_comp_ext *comp_ext)
{
	int i;

	memset(name, 0, size);
	sprintf(name, "%s", COMP_PREFIX);
	name += strlen(COMP_PREFIX);

	/* convert UUID char at a time */
	for (i = 0; i < 16; i++) {
		sprintf(name, "%2.2x", comp_ext->uuid[i]);
		name += 2;
	}
	sprintf(name, "%s", COMP_SUFFIX);
}

/* 72cee996-39f2-11ed-a08f-97fcc42eaaeb */
static const uint8_t virt_dai_playback_alsa[] =
	{0x96, 0xe9, 0xce, 0x72, 0xf2, 0x39, 0xed, 0x11,
	0xa0, 0x8f, 0x97, 0xfc, 0xc4, 0x2e, 0xaa, 0xeb};

/* 66def9f0-39f2-11ed-89f7-af98a6440cc4 */
static const uint8_t virt_dai_capture_alsa[] =
	{0xf0, 0xf9, 0xde, 0x66, 0xf2, 0x39, 0xed, 0x11,
	0xf7, 0x89, 0xaf, 0x98, 0xa6, 0x44, 0x0c, 0xc4};

/* 1488beda-e847-ed11-b309-a58b974fecce */
static const uint8_t virt_shm_read[] =
	{0x14, 0x88, 0xbe, 0xda, 0xe8, 0x47, 0xed, 0x11,
	0xa5, 0x8b, 0xb3, 0x09, 0x97, 0x4f, 0xec, 0xce};

/* 1c03b6e2-e847-ed11-7f80-07a91b6efa6c */
static const uint8_t virt_shm_write[] =
	{0x1c, 0x03, 0xb6, 0xe2, 0xe8, 0x47, 0xed, 0x11,
	0x07, 0xa9, 0x7f, 0x80, 0x1b, 0x6e, 0xfa, 0x6c};


static int pipe_register_comp(struct sof_pipe *sp,
		struct sof_ipc_comp *comp,
		struct sof_ipc_comp_ext *comp_ext)
{
	char uuid_sofile[NAME_SIZE];
	struct sof_ipc_comp_file *file;
	int i;

	/* determine module uuid */
	switch (comp->type) {
	case SOF_COMP_HOST:
		/* HOST is the sof-pipe SHM */
		file = (struct sof_ipc_comp_file *)comp;
		if (file->direction == 0)
			memcpy(comp_ext, virt_shm_write, 16);
		else
			memcpy(comp_ext, virt_shm_read, 16);
		break;
	case SOF_COMP_DAI:
		/* DAI is either ALSA device or file */
		file = (struct sof_ipc_comp_file *)comp;
		if (file->direction == 0)
			memcpy(comp_ext, virt_dai_playback_alsa, 16);
		else
			memcpy(comp_ext, virt_dai_capture_alsa, 16);
		break;
	default:
		/* dont care */
		break;
	}

	/* TODO: try other paths */
	pipe_comp_uuid(uuid_sofile, NAME_SIZE, comp_ext);

	/* check if module already loaded */
	for (i = 0; i < sp->mod_idx; i++) {
		if (!memcmp(sp->module[i].uuid, comp_ext->uuid, sizeof(*comp_ext->uuid))) {
			return 0; /* module found and already loaded */
		}
	}

	/* not loaded, so load module */
	sp->module[sp->mod_idx].handle = dlopen(uuid_sofile, RTLD_NOW);
	if (!sp->module[sp->mod_idx].handle) {
		fprintf(stderr, "error: cant load module %s: %s\n",
			uuid_sofile, dlerror());
		return -errno;
	}

	memcpy(sp->module[sp->mod_idx].uuid, comp_ext->uuid, sizeof(*comp_ext->uuid));
	sp->mod_idx++;

	return 0;
}

/* some components need loaded via UUID as shared objects */
static int pipe_comp_new(struct sof_pipe *sp, struct sof_ipc_cmd_hdr *hdr)
{
	struct sof_ipc_comp *comp = (struct sof_ipc_comp *)hdr;
	struct sof_ipc_comp_ext *comp_ext;

	if (comp->ext_data_length == 0) {
		fprintf(stderr, "error: no uuid for hdr 0x%x\n", hdr->cmd);
		return -EINVAL;
	}

	/* comp ext is at the end of the IPC structure */
	comp_ext = (struct sof_ipc_comp_ext *)(((char *)comp) + hdr->size - sizeof(struct sof_ipc_comp_ext));

	return pipe_register_comp(sp, comp, comp_ext);
}

static int pipe_comp_free(struct sof_pipe *sp, struct sof_ipc_cmd_hdr *hdr)
{
	return 0;
}

#if 0
static int pipe_ready_endpoints(struct sof_pipe *sp)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *ipc_ppl_source;
	struct ipc_comp_dev *ipc_ppl_sink;

	/* get pipeline source component */
	ipc_ppl_source = ipc_get_ppl_src_comp(ipc, p->pipeline_id);
	if (!ipc_ppl_source) {
		tr_err(&ipc_tr, "ipc: ipc_pipeline_complete looking for pipeline source failed");
		return -EINVAL;
	}

	/* get pipeline sink component */
	ipc_ppl_sink = ipc_get_ppl_sink_comp(ipc, p->pipeline_id);
	if (!ipc_ppl_sink) {
		tr_err(&ipc_tr, "ipc: ipc_pipeline_complete looking for pipeline sink failed");
		return -EINVAL;
	}
}
#endif

#define iCS(x) ((x) & SOF_CMD_TYPE_MASK)

static int ipc_tplg_message_before(struct sof_pipe *sp, void *mailbox,
				     size_t bytes)
{
	struct sof_ipc_cmd_hdr *hdr = mailbox;
	struct ipc *ipc = ipc_get();
	struct sof_ipc_free *ipc_free = (struct sof_ipc_free *)hdr;
	struct ipc_comp_dev *ipc_pipe;
	uint32_t cmd = iCS(hdr->cmd);
	int err;

	/* pipeline has to perform some work prior to core */
	switch (cmd) {
	case SOF_IPC_TPLG_COMP_NEW:
		return pipe_comp_new(sp, hdr);
	case SOF_IPC_TPLG_COMP_FREE:
		return pipe_comp_free(sp, hdr);
	case SOF_IPC_TPLG_PIPE_FREE:
		ipc_pipe = ipc_get_comp_by_id(ipc, ipc_free->id);
		if (!ipc_pipe) {
			printf("error: no component with ID %d\n",
					ipc_free->id);
			return -ENODEV;
		}
		if (ipc_pipe->type != COMP_TYPE_PIPELINE) {
			printf("error: no pipeline with ID %d got type %d\n",
				ipc_free->id, ipc_pipe->id);
			return -EINVAL;
		}
		err = pipe_thread_free(sp, ipc_pipe->pipeline);
		if (err < 0) {
			printf("error: can't free pipeline %d thread\n",
				ipc_pipe->pipeline->pipeline_id);
			return err;
		}

		return 0;
	default:
		/* handled directly by SOF core */
		return 0;
	}
}

static int ipc_tplg_message_after(struct sof_pipe *sp, void *mailbox,
				     size_t bytes)
{
	struct sof_ipc_cmd_hdr *hdr = mailbox;
	struct sof_ipc_pipe_new *pipe = (struct sof_ipc_pipe_new *)hdr;
	struct ipc_comp_dev *ipc_pipe;
	struct ipc *ipc = ipc_get();
	uint32_t cmd = iCS(hdr->cmd);
	int err;

	/* pipeline has to perform some work prior to core */
	switch (cmd) {
	case SOF_IPC_TPLG_PIPE_NEW:
		ipc_pipe = ipc_get_comp_by_id(ipc, pipe->comp_id);
		if (!ipc_pipe) {
			printf("error: no component with ID %d\n",
					pipe->comp_id);
			return -ENODEV;
		}
		if (ipc_pipe->type != COMP_TYPE_PIPELINE) {
			printf("error: no pipeline with ID %d got type %d\n",
				pipe->comp_id, ipc_pipe->id);
			return -EINVAL;
		}

		/* create new pipeline thread */
		err = pipe_thread_new(sp, ipc_pipe->pipeline, pipe->comp_id,
				pipe->pipeline_id);
		if (err < 0) {
			printf("error: can't create pipeline %d thread\n",
				ipc_pipe->pipeline->pipeline_id);
			return err;
		}

		return 0;
	default:
		/* handled directly by SOF core */
		return 0;
	}
}

static int ipc_tplg_stream_before(struct sof_pipe *sp, void *mailbox,
				     size_t bytes)
{
	struct sof_ipc_cmd_hdr *hdr = mailbox;
	struct sof_ipc_stream *stream = (struct sof_ipc_stream *)hdr;
	struct ipc_comp_dev *ipc_pipe;
	struct ipc *ipc = ipc_get();
	uint32_t cmd = iCS(hdr->cmd);
	int err;

	/* pipeline has to perform some work prior to core */
	switch (cmd) {
	case SOF_IPC_STREAM_TRIG_STOP:
		ipc_pipe = ipc_get_comp_by_id(ipc, stream->comp_id);
		if (!ipc_pipe) {
			printf("error: no component with ID %d\n", stream->comp_id);
			return -ENODEV;
		}

		/* stop the pipeline thread */
		err = pipe_thread_stop(sp, ipc_pipe->cd);
		if (err < 0) {
			printf("error: can't stop pipeline %d thread\n",
				ipc_pipe->pipeline->comp_id);
			return err;
		}

		return 0;
	default:
		/* handled directly by SOF core */
		return 0;
	}
}

static int ipc_tplg_stream_after(struct sof_pipe *sp, void *mailbox,
				     size_t bytes)
{
	struct sof_ipc_cmd_hdr *hdr = mailbox;
	struct sof_ipc_stream *stream = (struct sof_ipc_stream *)hdr;
	struct ipc_comp_dev *ipc_pipe;
	struct ipc *ipc = ipc_get();
	uint32_t cmd = iCS(hdr->cmd);
	int err;

	/* pipeline has to perform some work prior to core */
	switch (cmd) {
	case SOF_IPC_STREAM_TRIG_START:
		ipc_pipe = ipc_get_comp_by_id(ipc, stream->comp_id);
		if (!ipc_pipe) {
			printf("error: no component with ID %d\n", stream->comp_id);
			return -ENODEV;
		}

		/* start the pipeline thread */
		err = pipe_thread_start(sp, ipc_pipe->cd);
		if (err < 0) {
			printf("error: can't start pipeline %d thread\n",
				ipc_pipe->pipeline->comp_id);
			return err;
		}

		return 0;
	default:
		/* handled directly by SOF core */
		return 0;
	}
}

#define iGS(x) ((x) & SOF_GLB_TYPE_MASK)

static int pipe_sof_ipc_cmd_before(struct sof_pipe *sp, void *mailbox, size_t bytes)
{
	struct sof_ipc_cmd_hdr *hdr = mailbox;
	uint32_t type = iGS(hdr->cmd);
	int ret = 0;

	switch (type) {
	case SOF_IPC_GLB_REPLY:
		ret = 0;
		break;
	case SOF_IPC_GLB_COMPOUND:
		ret = -EINVAL;	/* TODO */
		break;
	case SOF_IPC_GLB_TPLG_MSG:
		ret = ipc_tplg_message_before(sp, mailbox, bytes);
		break;
	case SOF_IPC_GLB_STREAM_MSG:
		ret = ipc_tplg_stream_before(sp, mailbox, bytes);
		break;
	case SOF_IPC_GLB_PM_MSG:
	case SOF_IPC_GLB_COMP_MSG:
	case SOF_IPC_GLB_DAI_MSG:
	case SOF_IPC_GLB_TRACE_MSG:
	case SOF_IPC_GLB_GDB_DEBUG:
	case SOF_IPC_GLB_PROBE:
	case SOF_IPC_GLB_DEBUG:
		break;
#if CONFIG_DEBUG
	case SOF_IPC_GLB_TEST:
		ret = ipc_glb_test_message(hdr->cmd);
		break;
#endif
	default:
		fprintf(sp->log, "ipc: unknown command type %u size %ld",
			type, bytes);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int pipe_sof_ipc_cmd_after(struct sof_pipe *sp, void *mailbox, size_t bytes)
{
	struct sof_ipc_cmd_hdr *hdr = mailbox;
	uint32_t type = iGS(hdr->cmd);
	int ret = 0;

	switch (type) {
	case SOF_IPC_GLB_REPLY:
		ret = 0;
		break;
	case SOF_IPC_GLB_COMPOUND:
		ret = -EINVAL;	/* TODO */
		break;
	case SOF_IPC_GLB_TPLG_MSG:
		ret = ipc_tplg_message_after(sp, mailbox, bytes);
		break;
	case SOF_IPC_GLB_STREAM_MSG:
		ret = ipc_tplg_stream_after(sp, mailbox, bytes);
		break;
	case SOF_IPC_GLB_PM_MSG:
	case SOF_IPC_GLB_COMP_MSG:
	case SOF_IPC_GLB_DAI_MSG:
	case SOF_IPC_GLB_TRACE_MSG:
	case SOF_IPC_GLB_GDB_DEBUG:
	case SOF_IPC_GLB_PROBE:
	case SOF_IPC_GLB_DEBUG:
		break;
#if CONFIG_DEBUG
	case SOF_IPC_GLB_TEST:
		ret = ipc_glb_test_message(hdr->cmd);
		break;
#endif
	default:
		fprintf(sp->log, "ipc: unknown command type %u size %ld",
			type, bytes);
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

int pipe_ipc_process(struct sof_pipe *sp, struct plug_mq_desc *mq)
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
	err = plug_mq_create(mq);
	if (err < 0) {
		fprintf(sp->log, "error: can't create PCM IPC message queue : %s\n",
			strerror(errno));
		return -errno;
	}

	/* let main() know we are ready */
	fprintf(sp->log, "sof-pipe: IPC %s thread ready\n", mq->queue_name);

	/* main PCM IPC handling loop */
	while (1) {

		memset(mailbox, 0, IPC3_MAX_MSG_SIZE);

		/* is client dead ? */
		if (sp->glb->state == SOF_PLUGIN_STATE_DEAD) {
			fprintf(sp->log, "sof-pipe: IPC %s client complete\n", mq->queue_name);
			break;
		}

		ipc_size = mq_receive(mq->mq, mailbox, IPC3_MAX_MSG_SIZE, NULL);
		if (ipc_size < 0) {
			fprintf(sp->log, "error: can't read PCM IPC message queue %s : %s\n",
				sp->ipc_mq.queue_name, strerror(errno));
			break;
		}

		/* TODO: properly validate message and continue if garbage */
		if (*((uint32_t*)mailbox) == 0) {
			fprintf(sp->log, "sof-pipe: IPC %s garbage read\n", mq->queue_name);
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
		err = mq_send(mq->mq, mailbox, IPC3_MAX_MSG_SIZE, 0);
		if (err < 0) {
			fprintf(sp->log, "error: can't send PCM IPC message queue %s : %s\n",
				mq->queue_name, strerror(errno));
			break;
		}
	}

	fprintf(sp->log, "***sof-pipe: IPC %s thread finished !!\n", mq->queue_name);
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
