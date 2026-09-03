// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.

#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <rtos/symbol.h>

/*
 * Stubs that are needed for linkage of some applications or libraries
 * that come from porting userspace code. Anyone porting should
 * make sure that any code does not depend on working copies of these
 * reentrant functions. We will fail for any caller.
 */

struct stat;
struct _reent;

size_t _read_r(struct _reent *ptr, int fd, char *buf, size_t cnt)
{
	errno = -ENOTSUP;
	return -ENOTSUP;
}

size_t _write_r(struct _reent *ptr, int fd, char *buf, size_t cnt)
{
	errno = -ENOTSUP;
	return -ENOTSUP;
}

void *_sbrk_r(struct _reent *ptr, ptrdiff_t incr)
{
	errno = -ENOTSUP;
	return NULL;
}

int _lseek_r(struct _reent *ptr, int fd, int pos, int whence)
{
	errno = -ENOTSUP;
	return -ENOTSUP;
}

int _kill_r(struct _reent *ptr, int pid, int sig)
{
	errno = -ENOTSUP;
	return -ENOTSUP;
}

int _getpid_r(struct _reent *ptr)
{
	errno = -ENOTSUP;
	return -ENOTSUP;
}

int _fstat_r(struct _reent *ptr, int fd, struct stat *pstat)
{
	errno = -ENOTSUP;
	return -ENOTSUP;
}

int _close_r(struct _reent *ptr, int fd)
{
	errno = -ENOTSUP;
	return -ENOTSUP;
}

/* TFLM needs exit if build as a llext module only atm */
#if CONFIG_COMP_TENSORFLOW == m
void _exit(int status)
{
	assert(0);
	while (1) {
		/* spin forever */
	}
	/* NOTREACHED */
}
#endif
