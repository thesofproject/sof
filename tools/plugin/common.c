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

