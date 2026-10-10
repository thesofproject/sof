// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.

#include <sys/types.h>
#include <stdint.h>
#include <errno.h>
#include <rtos/symbol.h>

/*
 * Stubs that are needed for linkage of some applications or libraries
 * that come from porting userspace code. Anyone porting should
 * make sure that any code does not depend on working copies of these
 * reentrant functions. We will fail for any caller.
 */

struct stat;
struct _reent;

ssize_t _read_r(struct _reent *ptr, int fd, void *buf, size_t cnt)
{
	errno = ENOTSUP;
	return -1;
}

ssize_t _write_r(struct _reent *ptr, int fd, const void *buf, size_t cnt)
{
	errno = ENOTSUP;
	return -1;
}

void *_sbrk_r(struct _reent *ptr, ptrdiff_t incr)
{
	errno = ENOTSUP;
	return (void *)-1;
}

int _lseek_r(struct _reent *ptr, int fd, int pos, int whence)
{
	errno = ENOTSUP;
	return -1;
}

int _kill_r(struct _reent *ptr, int pid, int sig)
{
	errno = ENOTSUP;
	return -1;
}

int _getpid_r(struct _reent *ptr)
{
	errno = ENOTSUP;
	return -1;
}

int _fstat_r(struct _reent *ptr, int fd, struct stat *pstat)
{
	errno = ENOTSUP;
	return -1;
}

int _close_r(struct _reent *ptr, int fd)
{
	errno = ENOTSUP;
	return -1;
}

/*
 * libc.a's fmaxf()/fminf() are not built PIC-safe: their error path
 * (__math_invalidf()) calls __isnanf()/__issignalingf(), and GNU ld refuses
 * those relocations ("dangerous relocation") when linking this ET_DYN/PIC
 * LLEXT. TFLM's kernels use fmaxf()/fminf() for plain (non-NaN-signaling)
 * min/max, so provide direct, self-contained definitions here: linked ahead
 * of -lm, these satisfy the symbols outright and libm.a's versions (and the
 * non-PIC-safe helpers they pull in) are never extracted.
 */
float fmaxf(float x, float y)
{
	if (x != x)
		return y;
	if (y != y)
		return x;
	return (x > y) ? x : y;
}

float fminf(float x, float y)
{
	if (x != x)
		return y;
	if (y != y)
		return x;
	return (x < y) ? x : y;
}

/*
 * libc.a's shared float-domain-error helper __math_invalidf() (used by
 * logf()/sqrtf()/powf()/and most other single-precision libm functions'
 * error paths) is itself built non-PIC-safe: it calls __isnanf() via a
 * direct call8, which GNU ld refuses ("dangerous relocation") once that
 * non-PIC object is pulled into this ET_DYN/PIC LLEXT. Providing our own
 * definition here -- linked ahead of -lm -- means libc.a's copy (and the
 * relocation it can't satisfy) is never extracted, while every libm
 * function that calls it keeps working via its normal linked-in code.
 * Semantics mirror libc.a's own implementation: propagate x via x+x for a
 * NaN input (quieting/signalling per IEEE 754), else synthesize NaN via
 * 0.0f/0.0f.
 */
float __math_invalidf(float x)
{
	union { float f; uint32_t u; } v = { .f = x };
	int is_nan = (v.u & 0x7f800000u) == 0x7f800000u && (v.u & 0x007fffffu);

	if (is_nan)
		return x + x;

	return 0.0f / 0.0f;
}

/* TFLM needs exit if build as a llext module only atm */
#if CONFIG_COMP_MWW == m
void _exit(int status)
{
	/*
	 * Do not call libc assert() here: it drags in __assert_no_args(),
	 * which calls fwrite()/abort() from libc.a. Those objects are not
	 * built PIC-safe, and linking them into this ET_DYN/PIC LLEXT
	 * triggers GNU ld "dangerous relocation" errors. The spin loop
	 * below already provides the required "never return" behaviour.
	 */
	while (1) {
		/* spin forever */
	}
	/* NOTREACHED */
}
#endif
