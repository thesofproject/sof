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

#include "common.h"
#include "pipe.h"

#define VERSION		"v0.1"

/* global needed for signal handler */
struct sof_pipe *_sp;

static void shutdown(struct sof_pipe *sp)
{
	struct pipethread_data *pipeline_ctx = sp->pipeline_ctx;
	int i;

	/* free everything */
	munmap(sp->shm_context.addr, sp->shm_context.size);
	shm_unlink(sp->shm_context.name);

	/* cancel all threads, free locks and message queues */
	for (i = 0; i < sp->pipe_thread_count; i++) {
		struct pipethread_data *pd = &pipeline_ctx[i];

		pthread_cancel(pd->ipc_thread);
		pthread_cancel(pd->pcm_thread);
		plug_mq_free(&pd->ipc_tx_mq);
		plug_mq_free(&pd->ipc_rx_mq);
		plug_lock_free(&pd->ready);
		plug_lock_free(&pd->done);
	}

	/* free the sof-pipe IPC tx/rx message queues */
	plug_mq_free(&sp->ipc_tx_mq);
	plug_mq_free(&sp->ipc_rx_mq);

	pthread_mutex_destroy(&sp->ipc_lock);

	fflush(sp->log);
	fflush(stdout);
	fflush(stderr);
}

/* signals from the ALSA PCM plugin or something has gone wrong */
static void signal_handler(int sig)
{
	switch (sig) {
	case SIGTERM:
		fprintf(_sp->log, "Pipe caught SIGTERM - shutdown\n");
		break;
	case SIGINT:
		fprintf(_sp->log, "Pipe caught SIGINT - shutdown\n");
		break;
	default:
		fprintf(_sp->log, "Pipe caught signal %d, something went wrong\n", sig);
		break;
	}
	fprintf(_sp->log, "Pipe shutdown signal\n");

	/* try and clean up if we can */
	shutdown(_sp);
	exit(EXIT_FAILURE);
}

static int pipe_init_signals(struct sof_pipe *sp)
{
	struct sigaction *action = &sp->action;
	int err;

	/*
	 * signals - currently only check for SIGCHLD. TODO: handle more
	 */
	sigemptyset(&action->sa_mask);
	action->sa_handler = signal_handler;
	err = sigaction(SIGTERM, action, NULL);
	if (err < 0) {
		fprintf(sp->log, "failed to register signal action: %s",
			strerror(errno));
		return err;
	}

	err = sigaction(SIGSEGV, action, NULL);
	if (err < 0) {
		fprintf(sp->log, "failed to register signal action: %s",
			strerror(errno));
		return err;
	}

	err = sigaction(SIGINT, action, NULL);
	if (err < 0) {
		fprintf(sp->log, "failed to register signal action: %s",
			strerror(errno));
		return err;
	}

	return 0;
}

int pipe_ipc_message(struct sof_pipe *sp, void *mailbox, size_t bytes)
{
	struct ipc *ipc = ipc_get();

	/* reply is copied back to mailbox */
	pthread_mutex_lock(&sp->ipc_lock);
	memcpy(ipc->comp_data, mailbox, bytes);
	ipc_cmd(mailbox);
	memcpy(mailbox, ipc->comp_data, bytes);
	pthread_mutex_unlock(&sp->ipc_lock);

	return 0;
}

/*
 * Create and open a new semaphore using the lock object.
 */
int plug_lock_create(struct plug_sem_desc *lock)
{
	/* delete any old stale resources that use our resource name */
	sem_unlink(lock->name);

	/* RW blocking lock */
	lock->sem = sem_open(lock->name, O_CREAT | O_RDWR | O_EXCL, SEM_PERMS, 0);
	if (lock->sem == SEM_FAILED) {
		SNDERR("failed to create semaphore %s: %s", lock->name, strerror(errno));
		return -errno;
	}

	return 0;
}

/*
 * Free and delete semaphore resourses in lock object.
 */
void plug_lock_free(struct plug_sem_desc *lock)
{
	sem_close(lock->sem);
	sem_unlink(lock->name);
}

/*
 * Create and open a new shared memory region using the SHM object.
 */
int plug_shm_create(struct plug_shm_desc *shm)
{
	int err;

	/* delete any old stale resources that use our resource name */
	shm_unlink(shm->name);

	/* open SHM to be used for low latency position */
	shm->fd = shm_open(shm->name, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG);
	if (shm->fd < 0) {
		SNDERR("failed to create SHM position %s: %s", shm->name, strerror(errno));
		return -errno;
	}

	/* set SHM size */
	err = ftruncate(shm->fd, shm->size);
	if (err < 0) {
		SNDERR("failed to truncate SHM position %s: %s", shm->name, strerror(errno));
		shm_unlink(shm->name);
		return -errno;
	}

	/* map it locally for context readback */
	shm->addr = mmap(NULL, shm->size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
	if (!shm->addr) {
		SNDERR("failed to mmap SHM position%s: %s", shm->name, strerror(errno));
		shm_unlink(shm->name);
		return -errno;
	}

	return 0;
}

/*
 * Free and delete shared memory region resourses in SHM object.
 */
void plug_shm_free(struct plug_shm_desc *shm)
{
	close(shm->fd);
	shm_unlink(shm->name);
}

/*
 * Create and open a new message queue using the IPC object.
 */
int plug_mq_create(struct plug_mq_desc *ipc)
{
	/* delete any old stale resources that use our resource name */
	mq_unlink(ipc->queue_name);

	memset(&ipc->attr, 0, sizeof(ipc->attr));
	ipc->attr.mq_msgsize = IPC3_MAX_MSG_SIZE;
	ipc->attr.mq_maxmsg = 1;

	/* now open new queue for Tx/Rx */
	ipc->mq = mq_open(ipc->queue_name, O_CREAT | O_RDWR | O_EXCL,
			  S_IRWXU | S_IRWXG, &ipc->attr);
	if (ipc->mq < 0) {
		fprintf(stderr, "failed to create IPC queue %s: %s\n",
			ipc->queue_name, strerror(errno));
		return -errno;
	}

	return 0;
}

/*
 * Free and delete message queue resources in IPC object.
 */
void plug_mq_free(struct plug_mq_desc *ipc)
{
	mq_close(ipc->mq);
	mq_unlink(ipc->queue_name);
}

/*
 * -D ALSA device. e.g.
 * -R realtime (needs parent to set uid)
 * -p Force run on P core
 * -e Force run on E core
 * -t topology name.
 * -L log file (otherwise stdout)
 * -h help
 */
static void usage(char *name)
{
	fprintf(stdout, "Usage: %s -D ALSA device -T topology\n", name);
}

int main(int argc, char *argv[], char *env[])
{
	struct sof_pipe sp = {0};
	int option = 0;
	int ret = 0;

	/* default config */
	sp.log = stdout;
	sp.alsa_name = "default"; /* default sound device */
	_sp = &sp;

	/* parse all args */
	while ((option = getopt(argc, argv, "hD:RpeT:")) != -1) {
		switch (option) {
		/* Alsa device  */
		case 'D':
			sp.alsa_name = strdup(optarg);
			break;
		case 'R':
			sp.realtime = 1;
			break;
		case 'p':
			sp.use_P_core = 1;
			sp.use_E_core = 0;
			break;
		case 'e':
			sp.use_E_core = 1;
			sp.use_P_core = 0;
			break;
		case 'T':
			snprintf(sp.topology_name, NAME_SIZE, "%s", optarg);
			break;

		/* print usage */
		default:
			fprintf(sp.log, "unknown option %c\n", option);
			__attribute__ ((fallthrough));
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		}
	}

	/* validate cmd line params */
	if (strlen(sp.topology_name) == 0) {
		fprintf(sp.log, "error: no IPC topology name specified\n");
		exit(EXIT_FAILURE);
	}

	/* global IPC access serialisation mutex */
	ret = pthread_mutex_init(&sp.ipc_lock, NULL);
	if (ret < 0) {
		fprintf(sp.log, "error: cant create mutex %s\n",  strerror(errno));
		exit(EXIT_FAILURE);
	}

	fprintf(sp.log, "sof-pipe-%s: using topology %s\n", VERSION, sp.topology_name);

	/* set CPU affinity */
	if (sp.use_E_core || sp.use_P_core) {
		ret = pipe_set_affinity(&sp);
		if (ret < 0)
			goto out;
	}

	/* initialize ipc and scheduler */
	if (pipe_sof_setup(sof_get()) < 0) {
		fprintf(stderr, "error: pipeline init\n");
		exit(EXIT_FAILURE);
	}

	/* global context - plugin clients open this first */
	ret = plug_shm_init(&sp.shm_context, sp.topology_name, "ctx", 0);
	if (ret < 0)
		goto out;

	/* cleanup any lingering global IPC files */
	shm_unlink(sp.shm_context.name);

	/* make sure we can cleanly shutdown */
	ret = pipe_init_signals(&sp);
	if (ret < 0)
		goto out;

	/* mmap context on successful topology load */
	ret = plug_shm_create(&sp.shm_context);
	if (ret < 0)
		goto out;

	/* now prep the global context for client plugin access */
	sp.glb = sp.shm_context.addr;
	memset(sp.glb, 0, sizeof(*sp.glb));
	sprintf(sp.glb->magic, "%s", SOF_MAGIC);
	sp.glb->size = sizeof(*sp.glb);
	sp.glb->state = SOF_PLUGIN_STATE_INIT;
	sp.tplg.tplg_file = sp.topology_name;
	sp.tplg.ipc_major = 4; //HACK hard code to v4

	/* sofpipe is now ready */
	sp.glb->state = SOF_PLUGIN_STATE_INIT;

	ret = plug_mq_init(&sp.ipc_tx_mq, "sof", "ipc-tx", 0);
	if (ret < 0)
		goto out;

	ret = plug_mq_init(&sp.ipc_rx_mq, "sof", "ipc-rx", 0);
	if (ret < 0)
		goto out;

	/* now process IPCs as they arrive from plugins */
	ret = pipe_ipc_process(&sp, &sp.ipc_tx_mq, &sp.ipc_rx_mq);

out:
	fprintf(sp.log, "shutdown main\n");
	shutdown(&sp);
	return ret;
}
