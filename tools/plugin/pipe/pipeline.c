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
#include <dlfcn.h>

#include <rtos/sof.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <rtos/task.h>
#include <sof/lib/notifier.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>

#include "common.h"
#include "pipe.h"

#define MAX_PIPELINES	256

struct pipethread_data {
	pthread_t pcm_thread;
	pthread_t ipc_thread;
	struct sof_pipe *sp;
	struct plug_mq_desc ipc_mq;
	struct pipeline *pcm_pipeline;
	/* PCM flow control */
	struct plug_sem_desc ready;
	struct plug_sem_desc done;
};

static struct ll_schedule_domain domain = {0};

// TODO: all these steps are probably not needed - i.e we only need IPC and pipeline.
int pipe_sof_setup(struct sof *sof)
{
	/* register modules */

	domain.next_tick = 0;

	/* init components */
	sys_comp_init(sof);

	/* other necessary initializations, todo: follow better SOF init */
	pipeline_posn_init(sof);
	init_system_notify(sof);

	/* init IPC */
	if (ipc_init(sof) < 0) {
		fprintf(stderr, "error: IPC init\n");
		return -EINVAL;
	}

	/* init LL scheduler */
	if (scheduler_init_ll(&domain) < 0) {
		fprintf(stderr, "error: edf scheduler init\n");
		return -EINVAL;
	}

	/* init EDF scheduler */
	if (scheduler_init_edf() < 0) {
		fprintf(stderr, "error: edf scheduler init\n");
		return -EINVAL;
	}

	return 0;
}


static struct pipethread_data pipeline_ctx[MAX_PIPELINES] = {0};

static inline int pipe_copy_ready(struct pipethread_data *pd)
{
	struct timespec delay;
	int err;

	/* get the current time for source delay */
	err = clock_gettime(CLOCK_REALTIME, &delay);
	if (err == -1) {
		fprintf(_sp->log, "shm: cant get time: %s",
				strerror(errno));
		return -errno;
	}

	// TODO get from rate
	plug_timespec_add_ms(&delay, 200);

	/* wait for data from source */
	err = sem_timedwait(pd->ready.sem, &delay);
	if (err == -1) {
		fprintf(_sp->log, "pipe: fatal timeout: %s on %s\n",
				strerror(errno), pd->ready.name);
		return -errno;
	}

	return 0;
}

static inline void pipe_copy_done(struct pipethread_data *pd)
{
	/* tell peer we are done */
	sem_post(pd->done.sem);
}

static void *pipe_process_thread(void *arg)
{
	struct pipethread_data *pd = arg;
	int err;

	fprintf(_sp->log, "pipe thread started for pipeline %d\n",
			pd->pcm_pipeline->pipeline_id);

	do {
		if (pd->pcm_pipeline->status != COMP_STATE_ACTIVE) {
			fprintf(_sp->log, "pipe state non active %d\n",
				pd->pcm_pipeline->status);
			break;
		}

		/* wait for pipe to be ready */
		err = pipe_copy_ready(pd);
		if (err < 0) {
			fprintf(_sp->log, "pipe ready timeout %d\n",
					pd->pcm_pipeline->pipeline_id);
			break;
		}

		/* sink has read data so now generate more it */
		err = pipeline_copy(pd->pcm_pipeline);

		pipe_copy_done(pd);

		if (err < 0) {
			fprintf(_sp->log, "pipe thread error %d\n", err);
			break;
		} else if (err > 0) {
			fprintf(_sp->log, "pipe thread complete %d\n", err);
			break;
		}

	} while (1);

	fprintf(_sp->log, "pipe complete for pipeline %d\n",
		pd->pcm_pipeline->pipeline_id);
	return 0;
}

static void *pipe_ipc_process_thread(void *arg)
{
	struct pipethread_data *pd = arg;
	int err;

	err = pipe_ipc_process(pd->sp, &pd->ipc_mq);
	if (err < 0) {
		fprintf(_sp->log, "pipe IPC thread error for pipeline %d\n",
			pd->pcm_pipeline->pipeline_id);
	}

	return NULL;
}

int pipe_thread_start(struct sof_pipe *sp, struct comp_dev *cd)
{
	struct pipethread_data *pd;
	int pipeline_id;
	int ret;

	if (!cd) {
		fprintf(_sp->log, "error: invalid pipeline\n");
		return -EINVAL;
	}

	pipeline_id =cd->ipc_config.pipeline_id;

	if (pipeline_id >= MAX_PIPELINES) {
		fprintf(_sp->log, "error: pipeline ID %d out of range\n", pipeline_id);
		return -EINVAL;
	}

	if (!pipeline_ctx[pipeline_id].sp) {
		fprintf(_sp->log, "error: pipeline ID %d not in use\n", pipeline_id);
		return -EINVAL;
	}
	pd = &pipeline_ctx[pipeline_id];

	/* start PCM pipeline thread */
	ret = pthread_create(&pd->pcm_thread, NULL, pipe_process_thread, pd);
	if (ret < 0) {
		fprintf(_sp->log, "failed to create PCM thread: %s\n",
				strerror(errno));
		return -errno;
	}

	return ret;
}

int pipe_thread_stop(struct sof_pipe *sp, struct comp_dev *cd)
{
	struct pipethread_data *pd;
	int pipeline_id;
	int ret;

	if (!cd) {
		fprintf(_sp->log, "error: invalid pipeline\n");
		return -EINVAL;
	}

	pipeline_id = cd->ipc_config.pipeline_id;

	if (pipeline_id >= MAX_PIPELINES) {
		fprintf(_sp->log, "error: pipeline ID %d out of range\n", pipeline_id);
		return -EINVAL;
	}

	if (!pipeline_ctx[pipeline_id].sp) {
		fprintf(_sp->log, "error: pipeline ID %d not in use\n", pipeline_id);
		return -EINVAL;
	}
	pd = &pipeline_ctx[cd->ipc_config.id];

	ret = pthread_join(pd->pcm_thread, NULL);
	if (ret < 0) {
		fprintf(_sp->log, "failed to join PCM thread: %s\n",
				strerror(errno));
		return -errno;
	}

	return ret;
}

int pipe_thread_new(struct sof_pipe *sp, struct pipeline *p,
		unsigned int comp_id, unsigned int pipeline_id)
{
	struct pipethread_data *pd;
	int ret;

	if (!p) {
		fprintf(_sp->log, "error: invalid pipeline\n");
		return -EINVAL;
	}

	if (p->pipeline_id >= MAX_PIPELINES) {
		fprintf(_sp->log, "error: pipeline ID %d out of range\n", p->pipeline_id);
		return -EINVAL;
	}

	if (pipeline_ctx[p->pipeline_id].sp) {
		fprintf(_sp->log, "error: pipeline ID %d in use\n", p->pipeline_id);
		return -EINVAL;
	}
	pd = &pipeline_ctx[p->pipeline_id];
	pd->sp = _sp;
	pd->pcm_pipeline = p;

	/* initialise global IPC data */
	/* TODO: change the PCM name to tplg or make it per PID*/
	ret = plug_mq_init(&pd->ipc_mq, pd->sp->topology_name, "pcm", p->pipeline_id);
	if (ret < 0)
		return -EINVAL;
	mq_unlink(pd->ipc_mq.queue_name);

	/* init names of shared resources */
	ret = plug_lock_init(&pd->ready, _sp->topology_name, "ready",
			pipeline_id);
	if (ret < 0)
		return ret;

	ret = plug_lock_init(&pd->done, _sp->topology_name, "done",
			pipeline_id);
	if (ret)
		return ret;

	/* open semaphores */
	ret = plug_lock_create(&pd->ready);
	if (ret < 0)
		return ret;
	ret = plug_lock_create(&pd->done);
	if (ret < 0)
		goto lock_err;

	/* start IPC pipeline thread */
	ret = pthread_create(&pd->ipc_thread, NULL, pipe_ipc_process_thread, pd);
	if (ret < 0) {
		fprintf(_sp->log, "failed to create IPC thread: %s\n",
				strerror(errno));
		ret = -errno;
		goto lock2_err;
	}

	return 0;

lock2_err:
	plug_lock_free(&pd->done);
lock_err:
	plug_lock_free(&pd->ready);
	return ret;
}

int pipe_thread_free(struct sof_pipe *sp, struct pipeline *p)
{
	struct pipethread_data *pd;
	int err;

	if (p->pipeline_id >= MAX_PIPELINES) {
		fprintf(_sp->log, "error: pipeline ID %d out of range\n", p->pipeline_id);
		return -EINVAL;
	}

	if (!pipeline_ctx[p->pipeline_id].sp) {
		fprintf(_sp->log, "error: pipeline ID %d not in use\n", p->pipeline_id);
		return -EINVAL;
	}

	pd = &pipeline_ctx[p->pipeline_id];

	err = pthread_join(pd->ipc_thread, NULL);
	if (err < 0) {
		fprintf(_sp->log, "failed to create IPC thread: %s\n",
				strerror(errno));
		return -errno;
	}

	plug_mq_free(&pd->ipc_mq);
	mq_unlink(pd->ipc_mq.queue_name);

	plug_lock_free(&pd->ready);
	plug_lock_free(&pd->done);

	pd->sp = NULL;
	return 0;
}
