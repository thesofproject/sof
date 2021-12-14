// SPDX-License-Identifier: BSD-3-Clause
//
// Those functions are stubs and implemented to ease linking libraries relying on
// certain operating system symbols to be present at link time.
// Those stub are not meant to be called at runtime and will panic if called.

#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sof/debug/panic.h>
#include <ipc/trace.h>

struct _reent;

ssize_t _write_r(struct _reent *ptr,
				  int fd,
				  const void *buf,
				  size_t cnt);
ssize_t _write_r(struct _reent *ptr,
				  int fd,
				  const void *buf,
				  size_t cnt)
{
	panic(SOF_IPC_PANIC_ARCH);
	return 0;
}

off_t _lseek_r(struct _reent *ptr,
				int fd,
				off_t pos,
				int whence);
off_t _lseek_r(struct _reent *ptr,
				int fd,
				off_t pos,
				int whence)
{
	off_t ret;

	panic(SOF_IPC_PANIC_ARCH);
	return ret;
}

int _kill_r(struct _reent *ptr,
			 int pid,
			 int sig);
int _kill_r(struct _reent *ptr,
			 int pid,
			 int sig)
{
	panic(SOF_IPC_PANIC_ARCH);
	return 0;
}

void *_sbrk_r(struct _reent *ptr,
			  ptrdiff_t incr);
void *_sbrk_r(struct _reent *ptr,
			  ptrdiff_t incr)
{
	panic(SOF_IPC_PANIC_ARCH);
	return NULL;
}

void _exit(int __status);
void _exit(int __status)
{
	panic(SOF_IPC_PANIC_ARCH);
}


ssize_t _read_r(struct _reent *ptr,
				 int fd,
				 void *buf,
				 size_t cnt);
ssize_t _read_r(struct _reent *ptr,
				 int fd,
				 void *buf,
				 size_t cnt)
{
	panic(SOF_IPC_PANIC_ARCH);
	return 0;
}

int _close_r(struct _reent *ptr, int fd);
int _close_r(struct _reent *ptr, int fd)
{
	panic(SOF_IPC_PANIC_ARCH);
	return 0;
}


int _getpid_r(struct _reent *ptr);
int _getpid_r(struct _reent *ptr)
{
	panic(SOF_IPC_PANIC_ARCH);
	return 0;
}

int _fstat_r(struct _reent *ptr,
			 int fd, struct stat *pstat);
int _fstat_r(struct _reent *ptr,
			 int fd, struct stat *pstat)
{
	panic(SOF_IPC_PANIC_ARCH);
	return -1;
}

