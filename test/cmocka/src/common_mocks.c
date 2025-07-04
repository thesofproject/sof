// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019-2020 Intel Corporation. All rights reserved.
//
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>
// Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>

#include <errno.h>
#include <rtos/alloc.h>
#include <rtos/timer.h>
#include <sof/lib/mm_heap.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/driver.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <user/trace.h>
#include <rtos/spinlock.h>
#include <sof/audio/component_ext.h>
#include <rtos/clk.h>
#include <sof/lib/notifier.h>
#include <rtos/wait.h>
#include <arch/lib/cpu.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#define WEAK __attribute__((weak))

/* global contexts */
WEAK struct ipc *_ipc;
WEAK struct timer *platform_timer;
WEAK struct schedulers *schedulers;
WEAK struct sof sof;
WEAK struct tr_ctx buffer_tr;
WEAK struct tr_ctx comp_tr;
WEAK struct tr_ctx ipc_tr;

int host_trace_level = LOG_LEVEL_ERROR;

void WEAK *rballoc_align(uint32_t flags, uint32_t caps, size_t bytes,
			 uint32_t alignment)
{
	(void)flags;
	(void)caps;

	return calloc(bytes, 1);
}

void WEAK *rzalloc(enum mem_zone zone, uint32_t flags, uint32_t caps,
		   size_t bytes)
{
	(void)zone;
	(void)flags;
	(void)caps;

	return calloc(bytes, 1);
}

void WEAK *rbrealloc_align(void *ptr, uint32_t flags, uint32_t caps,
			   size_t bytes, size_t old_bytes, uint32_t alignment)
{
	(void)flags;
	(void)caps;
	(void)old_bytes;

	return realloc(ptr, bytes);
}

void WEAK rfree(void *ptr)
{
	free(ptr);
}

int WEAK memcpy_s(void *dest, size_t dest_size,
		  const void *src, size_t count)
{
	if (!dest || !src)
		return -EINVAL;

	if ((dest >= src && (char *)dest < ((char *)src + count)) ||
	    (src >= dest && (char *)src < ((char *)dest + dest_size)))
		return -EINVAL;

	if (count > dest_size)
		return -EINVAL;

	memcpy(dest, src, count);

	return 0;
}

int WEAK memset_s(void *dest, size_t size1,
		  int c, size_t size2)
{
	if (!dest)
		return -EINVAL;

	memset(dest, c, size1);

	return 0;
}

int WEAK rstrlen(const char *s)
{
	return strlen(s);
}

void WEAK __panic(uint32_t p, const char *filename, uint32_t linenum)
{
	fail_msg("panic: %s:%d (code 0x%x)\n", filename, linenum, p);
	exit(EXIT_FAILURE);
}

#if CONFIG_TRACE
void WEAK trace_log_filtered(bool send_atomic, const void *log_entry, const struct tr_ctx *ctx,
			     uint32_t lvl, uint32_t id_1, uint32_t id_2, int arg_count,
			     va_list args)
{
	(void) send_atomic;
	(void) log_entry;
	(void) ctx;
	(void) lvl;
	(void) id_1;
	(void) id_2;
	(void) arg_count;
	(void) args;
}

void WEAK _log_sofdict(log_func_t sofdict_logf, bool atomic, const void *log_entry,
		       const struct tr_ctx *ctx, const uint32_t lvl,
		       uint32_t id_1, uint32_t id_2, int arg_count, ...)
{
}

void WEAK trace_flush_dma_to_mbox(void)
{
}
#endif

#if CONFIG_LIBRARY
volatile void *task_context_get(void);
#endif
volatile void * WEAK task_context_get(void)
{
	return NULL;
}

uint32_t WEAK _k_spin_lock_irq(struct k_spinlock *lock)
{
	(void)lock;

	return 0;
}

void WEAK _k_spin_unlock_irq(struct k_spinlock *lock, uint32_t flags, int line)
{
	(void)lock;
	(void)flags;
	(void)line;
}

uint64_t WEAK platform_timer_get(struct timer *timer)
{
	(void)timer;

	return 0;
}

#if !CONFIG_LIBRARY
uint64_t WEAK arch_timer_get_system(struct timer *timer)
{
	(void)timer;

	return 0;
}
#endif

#if CONFIG_LIBRARY
void WEAK arch_dump_regs_a(void *dump_buf);
#endif
void WEAK arch_dump_regs_a(void *dump_buf)
{
	(void)dump_buf;
}

void WEAK heap_trace_all(int force)
{
	(void)force;
}

void WEAK ipc_msg_send(struct ipc_msg *msg, void *data, bool high_priority)
{
	(void)msg;
	(void)data;
	(void)high_priority;
}

int WEAK platform_ipc_init(struct ipc *ipc)
{
	return 0;
}

enum task_state WEAK ipc_platform_do_cmd(struct ipc *ipc)
{
	return 0;
}

void WEAK ipc_platform_complete_cmd(struct ipc *ipc)
{
}

#if !CONFIG_LIBRARY
int WEAK ipc_platform_send_msg(const struct ipc_msg *msg)
{
	return 0;
}

void WEAK wait_delay(uint64_t number_of_clks)
{
}

void WEAK wait_delay_ms(uint64_t ms)
{
}

void WEAK wait_delay_us(uint64_t us)
{
}

void WEAK xthal_icache_region_invalidate(void *addr, unsigned size)
{
}

void WEAK xthal_dcache_region_invalidate(void *addr, unsigned size)
{
}

void WEAK xthal_dcache_region_writeback(void *addr, unsigned size)
{
}

void WEAK xthal_dcache_region_writeback_inv(void *addr, unsigned size)
{
}
#endif

struct sof * WEAK sof_get(void)
{
	return &sof;
}

struct schedulers ** WEAK arch_schedulers_get(void)
{
	return &schedulers;
}

int WEAK schedule_task_init(struct task *task,
			    const struct sof_uuid_entry *uid, uint16_t type,
			    uint16_t priority, enum task_state (*run)(void *data),
			    void *data, uint16_t core, uint32_t flags)
{
	(void)task;
	(void)uid;
	(void)type;
	(void)priority;
	(void)run;
	(void)data;
	(void)core;
	(void)flags;

	return 0;
}

int WEAK schedule_task_init_ll(struct task *task,
			       const struct sof_uuid_entry *uid, uint16_t type,
			       uint16_t priority, enum task_state (*run)(void *data),
			       void *data, uint16_t core, uint32_t flags)
{
	return 0;
}

void WEAK platform_host_timestamp(struct comp_dev *host,
				  struct sof_ipc_stream_posn *posn)
{
	(void)host;
	(void)posn;
}

void WEAK platform_dai_timestamp(struct comp_dev *dai,
				 struct sof_ipc_stream_posn *posn)
{
	(void)dai;
	(void)posn;
}

struct ipc_comp_dev *WEAK ipc_get_comp_dev(struct ipc *ipc, uint16_t type, uint32_t id)
{
	(void)ipc;
	(void)type;
	(void)id;

	return NULL;
}

struct ipc_comp_dev *WEAK ipc_get_comp_by_ppl_id(struct ipc *ipc, uint16_t type,
						 uint32_t ppl_id, uint32_t ignore_remote)
{
	(void)ipc;
	(void)type;
	(void)ppl_id;
	(void)ignore_remote;

	return NULL;
}

uint32_t WEAK crc32(uint32_t base, const void *data, uint32_t bytes)
{
	return 0;
}

int WEAK comp_set_state(struct comp_dev *dev, int cmd)
{
	return 0;
}

uint64_t WEAK clock_ms_to_ticks(int clock, uint64_t ms)
{
	(void)clock;
	(void)ms;

	return 0;
}

uint64_t WEAK clock_us_to_ticks(int clock, uint64_t us)
{
	(void)clock;
	(void)us;

	return 0;
}

uint64_t WEAK clock_ns_to_ticks(int clock, uint64_t us)
{
	(void)clock;
	(void)us;

	return 0;
}

#if CONFIG_MULTICORE && !CONFIG_LIBRARY

int WEAK idc_send_msg(struct idc_msg *msg, uint32_t mode)
{
	(void)msg;
	(void)mode;

	return 0;
}

int WEAK arch_cpu_is_core_enabled(int id)
{
	return 0;
}

#endif

#if CONFIG_LIBRARY
/* enable trace by default in testbench */
int WEAK test_bench_trace = 1;
int WEAK debug;

/* ... but not always in unit tests */
#if CONFIG_TRACE
/* look up subsystem class name from table */
char * WEAK get_trace_class(uint32_t trace_class)
{
	(void)trace_class;
	/* todo: trace class is deprecated,
	 * uuid should be used only
	 */
	return "unknown";
}
#endif

uint8_t * WEAK get_library_mailbox(void)
{
	return NULL;
}
#endif

/*
 * GCC xtensa requires us to fake some of the standard C library calls
 * as this is "bare metal" support (i.e. with no host OS implementation
 * methods for these calls).
 *
 * None of these IO calls are used in the Mocks for testing.
 */
#if !CONFIG_LIBRARY && !__XCC__
void _exit(int status);
unsigned int _getpid_r(void);
int _kill_r(int id, int sig);
void _sbrk_r(int id);
int _fstat_r(int fd, void *buf);
int _open_r(const char *pathname, int flags);
int _write_r(int fd, char *buf, int count);
int _read_r(int fd, char *buf, int count);
int _lseek_r(int fd, int count);
int _close_r(int fd);
int _isatty(int fd);

void _exit(int status)
{
	while (1);
}

unsigned int _getpid_r(void)
{
	return 0;
}

int _kill_r(int id, int sig)
{
	return 0;
}

void _sbrk_r(int id)
{
}

int _fstat_r(int fd, void *buf)
{
	return 0;
}

int _open_r(const char *pathname, int flags)
{
	return 0;
}

int _write_r(int fd, char *buf, int count)
{
	return 0;
}

int _read_r(int fd, char *buf, int count)
{
	return 0;
}

int _lseek_r(int fd, int count)
{
	return 0;
}

int _close_r(int fd)
{
	return 0;
}

int _isatty(int fd)
{
	return 0;
}

/* TODO: work around some linker warnings. Both will need fixed for qemu. */
int _start = 0;
int *__errno _PARAMS ((void));

/*
 * TODO: Math support for GCC xtensa requires a little more work to use the
 * newlib versions. This is just to build test only today !
 */
double __floatsidf(int i);
double __divdf3(double a, double b);
int __ledf2(double a, double b);
int __eqdf2(double a, double b);

double __floatsidf(int i)
{
	return i;
}

double __divdf3(double a, double b)
{
	return a / b;
}

int WEAK __ledf2(double a, double b)
{
	return a <= b ? 0 : 1;
}

int WEAK __eqdf2(double a, double b)
{
	return a == b ? 0 : 1;
}
#endif
