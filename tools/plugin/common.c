/*-*- linux-c -*-*/

/*
 * ALSA <-> SOF PCM I/O plugin
 *
 * Copyright (c) 2022 by Liam Girdwood <liam.r.girdwood@intel.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "common.h"

/*
 * Timing
 */
void plug_timespec_add_ms(struct timespec *ts, unsigned long ms)
{
	long ns;
	long secs = ms / 1000;

	/* get ms remainder */
	ms = ms - (secs * 1000);
	ns = ms * 1000000;

	ts->tv_nsec += ns;
	if (ts->tv_nsec > 1000000000) {
		secs++;
		ts->tv_nsec -= 1000000000;
	}
	ts->tv_sec += (secs + DEBUG_TV_SECS);
}

long plug_timespec_delta_ns(struct timespec *before, struct timespec *after)
{
	long ns;

	ns = (after->tv_sec - before->tv_sec) * 1000000000;
	ns += after->tv_nsec - before->tv_nsec;

	return ns;
}

static const char *suffix_name(const char *longname)
{
	size_t len = strlen(longname);
	int i = len;

	/* longname name invalid */
	if (len < 1) {
		SNDERR("invalid topology long name\n");
		return NULL;
	}

	/* find the last '/' in the longname topology path */
	while (--i >= 0) {
		if (longname[i] == '/') {
			i += 1; /* skip / */
			return &longname[i];
		}
	}

	/* no / in topology path, so use full path */
	return longname;
}

/*
 * IPC
 *
 * POSIX message queues are used for interprocess IPC messaging.
 */

/*
 * Initialise the IPC object.
 */
int plug_mq_init(struct plug_mq_desc *ipc, const char *tplg, const char *type, int index)
{
	const char *name = suffix_name(tplg);

	if (!name)
		return -EINVAL;

	snprintf(ipc->queue_name, NAME_SIZE, "/mq-%s-%s-%d", type, name, index);
	return 0;
}

/*
 * Locking
 *
 * POSIX semaphores are used to block and synchronise audio between
 * different threads and processes.
 */

/*
 * Initialise the lock object.
 */
int plug_lock_init(struct plug_sem_desc *lock, const char *tplg, const char *type, int index)
{
	const char *name = suffix_name(tplg);

	if (!name)
		return -EINVAL;

	/* semaphores need the leading / */
	snprintf(lock->name, NAME_SIZE, "/lock-%s-%s-%d", name, type, index);

	return 0;
}

/*
 * SHM
 *
 * Shared memory is used for audio data and audio context sharing between
 * threads and processes.
 */

/*
 * Initialise the SHM object.
 */
int plug_shm_init(struct plug_shm_desc *shm, const char *tplg, const char *type, int index)
{
	const char *name = suffix_name(tplg);

	if (!name)
		return -EINVAL;

	snprintf(shm->name, NAME_SIZE, "/shm-%s-%s-%d", name, type, index);
	shm->size = SHM_SIZE;

	return 0;
}

/*
 * Open an existing shared memory region using the SHM object.
 */
int plug_shm_open(struct plug_shm_desc *shm)
{
	struct stat status;

	/* open SHM to be used for low latency position */
	shm->fd = shm_open(shm->name, O_RDWR,
			   S_IRWXU | S_IRWXG);
	if (shm->fd < 0) {
		//SNDERR("failed to open SHM position %s: %s\n",
		//	shm->name, strerror(errno));
		return -errno;
	}

	fstat(shm->fd, &status);
	/* map it locally for context readback */
	shm->addr = mmap(NULL, status.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
	if (!shm->addr) {
		SNDERR("failed to mmap SHM position%s: %s\n", shm->name, strerror(errno));
		return -errno;
	}

	return 0;
}

int plug_mq_cmd_tx_rx(struct plug_mq_desc *ipc_tx, struct plug_mq_desc *ipc_rx,
		      void *msg, size_t len, void *reply, size_t rlen)
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
	err = mq_timedsend(ipc_tx->mq, mailbox, IPC3_MAX_MSG_SIZE, 0, &ts);
	if (err == -1) {
		SNDERR("error: timeout can't send IPC message queue %s : %s\n",
		       ipc_tx->queue_name, strerror(errno));
		return -errno;
	}

	/* wait for sof-pipe reader to consume data or timeout */
	err = clock_gettime(CLOCK_REALTIME, &ts);
	if (err == -1) {
		SNDERR("ipc: cant get time: %s", strerror(errno));
		return -errno;
	}

	/* IPCs should be processed under 20ms, but wait longer as
	 * some can take longer especially in valgrind
	 */
	plug_timespec_add_ms(&ts, 20);

	ipc_size = mq_timedreceive(ipc_rx->mq, mailbox, IPC3_MAX_MSG_SIZE, NULL, &ts);
	if (ipc_size == -1) {
		//fprintf(stderr, "dbg: timeout can't read IPC message queue %s : %s retrying\n",
		//	ipc->queue_name, strerror(errno));

		/* ok, its a long IPC or valgrind, wait longer */
		plug_timespec_add_ms(&ts, 800);

		ipc_size = mq_timedreceive(ipc_rx->mq, mailbox, IPC3_MAX_MSG_SIZE, NULL, &ts);
		if (ipc_size == -1) {
			SNDERR("error: timeout can't read IPC message queue %s : %s\n",
			       ipc_rx->queue_name, strerror(errno));
			return -errno;
		}

		/* needed for valgrind to complete MQ op before next client IPC */
		ts.tv_nsec = 20 * 1000 * 1000;
		ts.tv_sec = 0;
		nanosleep(&ts, NULL);
	}

	/* do the message work */
	if (rlen && reply)
		memcpy(reply, mailbox, rlen);

	return 0;
}

void plug_ctl_ipc_message(struct ipc4_module_large_config *config, int param_id,
			  size_t size, uint32_t module_id, uint32_t instance_id,
			  uint32_t type)
{
	config->primary.r.type = type;
	config->primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
	config->primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	config->primary.r.module_id = module_id;
	config->primary.r.instance_id = instance_id;

	config->extension.r.data_off_size = size;
	config->extension.r.large_param_id = param_id;
}

int plug_send_bytes_data(struct plug_mq_desc *ipc_tx, struct plug_mq_desc *ipc_rx,
			 uint32_t module_id, uint32_t instance_id, struct sof_abi_hdr *abi)
{
	struct ipc4_module_large_config config = {{ 0 }};
	struct ipc4_message_reply reply;
	void *msg;
	int msg_size;
	int err;

	/* configure the IPC message */
	plug_ctl_ipc_message(&config, abi->type, abi->size, module_id, instance_id,
			     SOF_IPC4_MOD_LARGE_CONFIG_SET);

	config.extension.r.final_block = 1;
	config.extension.r.init_block = 1;

	/* allocate memory for IPC message */
	msg_size = sizeof(config) + abi->size;
	msg = calloc(msg_size, 1);
	if (!msg)
		return -ENOMEM;

	/* set the IPC message data */
	memcpy(msg, &config, sizeof(config));
	memcpy(msg + sizeof(config), abi->data, abi->size);

	/* send the message and check status */
	err = plug_mq_cmd_tx_rx(ipc_tx, ipc_rx, msg, msg_size, &reply, sizeof(reply));
	free(msg);
	if (err < 0) {
		SNDERR("failed to send IPC to set bytes data\n");
		return err;
	}

	if (reply.primary.r.status != IPC4_SUCCESS) {
		SNDERR("IPC failed with status %d\n", reply.primary.r.status);
		return -EINVAL;
	}

	return 0;
}
