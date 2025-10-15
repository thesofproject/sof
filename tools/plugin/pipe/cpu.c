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
#include <fcntl.h>
#include <sys/mman.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <limits.h>
#include <dlfcn.h>

#include "common.h"
#include "pipe.h"

/* read the CPU ID register data on x86 */
static inline void x86_cpuid(unsigned int *eax, unsigned int *ebx,
			     unsigned int *ecx, unsigned int *edx)
{
	/* data type is passed in on eax (and sometimes ecx) */
	asm volatile("cpuid"
		     : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
		     : "0" (*eax), "2" (*ecx));
}

/*
 * Check core type for E cores. If non hybrid then it does not matter.
 */
static inline int use_this_core(struct sof_pipe *sp)
{
	/* CPUID - set eax to 0x1a for hybrid core types */
	unsigned int eax = 0x1a, ebx = 0, ecx = 0, edx = 0;
	char core_mask;

	/* get the processor core type we are running on now */
	x86_cpuid(&eax, &ebx, &ecx, &edx);

	/* core type 0x20 is atom, 0x40 is core */
	core_mask = (eax >> 24) & 0xFF;
	switch (core_mask) {
	case 0x20:
		fprintf(sp->log, "found E core\n");
		if (sp->use_E_core)
			return 1;
		return 0;
	case 0x40:
		fprintf(sp->log, "found P core\n");
		if (sp->use_P_core)
			return 1;
		return 0;
	default:
		/* non hybrid arch, just use first core */
		fprintf(sp->log, "found non hybrid core topology\n");
		return 1;
	}
}

/* sof-pipe needs to be sticky to the current core for low latency */
int pipe_set_affinity(struct sof_pipe *sp)
{
	cpu_set_t cpuset;
	pthread_t thread;
	long core_count = sysconf(_SC_NPROCESSORS_ONLN);
	int i;
	int err;

	/* Set affinity mask to  core */
	thread = pthread_self();
	CPU_ZERO(&cpuset);

	/* find the first E core (usually come after the P cores ?) */
	for (i = core_count - 1; i >= 0; i--) {
		CPU_ZERO(&cpuset);
		CPU_SET(i, &cpuset);

		/* change our core to i */
		err = pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
		if (err != 0) {
			fprintf(sp->log, "error: failed to set CPU affinity to core %d: %s\n",
				i, strerror(err));
			return err;
		}

		/* should we use this core ? */
		if (use_this_core(sp))
			break;
	}

	return 0;
}

/* set ipc thread to low priority */
int pipe_set_ipc_lowpri(struct sof_pipe *sp)
{
	pthread_attr_t attr;
	struct sched_param param;
	int err;

	/* attempt to set thread priority - needs suid */
	fprintf(sp->log, "pipe: set IPC low priority\n");

	err = pthread_attr_init(&attr);
	if (err < 0) {
		fprintf(sp->log, "error: can't create thread attr %d %s\n", err, strerror(errno));
		return err;
	}

	err = pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
	if (err < 0) {
		fprintf(sp->log, "error: can't set thread policy %d %s\n", err, strerror(errno));
		return err;
	}
	param.sched_priority = 0;
	err = pthread_attr_setschedparam(&attr, &param);
	if (err < 0) {
		fprintf(sp->log, "error: can't set thread sched param %d %s\n",
			err, strerror(errno));
		return err;
	}

	return 0;
}

/* set pipeline to realtime priority */
int pipe_set_rt(struct sof_pipe *sp)
{
	pthread_attr_t attr;
	struct sched_param param;
	int err;
	uid_t uid = getuid();
	uid_t euid = geteuid();

	/* do we have elevated privileges to attempt RT priority */
	if (uid < 0 || uid != euid) {
		/* attempt to set thread priority - needs suid */
		fprintf(sp->log, "pipe: set RT priority\n");

		err = pthread_attr_init(&attr);
		if (err < 0) {
			fprintf(sp->log, "error: can't create thread attr %d %s\n",
				err, strerror(errno));
			return err;
		}

		err = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
		if (err < 0) {
			fprintf(sp->log, "error: can't set thread policy %d %s\n",
				err, strerror(errno));
			return err;
		}
		param.sched_priority = 80;
		err = pthread_attr_setschedparam(&attr, &param);
		if (err < 0) {
			fprintf(sp->log, "error: can't set thread sched param %d %s\n",
				err, strerror(errno));
			return err;
		}
		err = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
		if (err < 0) {
			fprintf(sp->log, "error: can't set thread inherit %d %s\n",
				err, strerror(errno));
			return err;
		}
	} else {
		fprintf(sp->log, "error: no elevated privileges for RT. uid %d euid %d\n",
			uid, euid);
	}

	return 0;
}
