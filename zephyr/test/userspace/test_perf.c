// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2026 Intel Corporation. */

/* Test kernel vs. user-space performance. */

#include <sof/boot_test.h>
#include <rtos/alloc.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

static int load_add(void)
{
#define N_ADD (1000 * 1000 * 100)
	unsigned long r = 0;

	for (unsigned int i = 0; i < N_ADD; i++)
		r += i;
#define N_DIV 10000
	for (unsigned int i = 1; i <= N_DIV; i++)
		r = r / (i % 10 + 1) * (i % 10 + 3);
	return (int)r;
}

#ifdef __XCC__
#include <xtensa/tie/xt_hifi4.h>

/* Compute dot product of two vectors using HiFi4 SIMD instructions */
static int32_t dot_product_hifi4(const int16_t *a, const int16_t *b, int length)
{
	ae_int64 acc = AE_ZERO64();		/* 1. Initialize accumulator to zero */
	ae_int16x4 *pa = (ae_int16x4 *)a;	/* Pointer to vector a */
	ae_int16x4 *pb = (ae_int16x4 *)b;	/* Pointer to vector b */

	for (int i = 0; i < length / 4; i++) {
		ae_int16x4 va, vb;

		AE_L16X4_IP(va, pa, 8);		/* 2. Load 4x 16-bit values from a */
		AE_L16X4_IP(vb, pb, 8);		/* 3. Load 4x 16-bit values from b */
		AE_MULAAAAQ16(acc, va, vb);	/* 4. Multiply-accumulate (4 MACs in parallel) */
	}

	return AE_TRUNCA32F64S(acc, 0);		/* 5. Convert 64-bit result to 32-bit */
}

#define VECTOR_LENGTH 100
static int load_hifi4(void)
{
	uint16_t a[VECTOR_LENGTH], b[VECTOR_LENGTH];
	int ret = 0;

	for (unsigned int j = 0; j < 1000; j++) {
		for (unsigned int i = 0; i < VECTOR_LENGTH; i++) {
			a[i] = i * 3 - 47 * j;
			b[i] = 411 * j - i * 5;
		}

		ret += dot_product_hifi4(a, b, VECTOR_LENGTH);
	}
	return ret;
}
#endif /* __XCC__ */

typedef int (*load_fn_t)(void);

load_fn_t load_fn[] = {
	load_add,
#ifdef __XCC__
	load_hifi4,
#endif
};

static unsigned int test_perf(load_fn_t fn, struct k_event *event,
			      struct k_sem *sem)
{
	uint64_t start = k_uptime_ticks();

	k_event_set(event, (uint32_t)fn);

	int ret = k_sem_take(sem, K_MSEC(200));

	zassert_ok(ret);

	uint64_t end = k_uptime_ticks();

	return (unsigned int)(end - start);
}

static void thread_fn(void *p1, void *p2, void *p3)
{
	struct k_event *event = p1;
	struct k_sem *sem = p2;
	bool first = true;

	for (;;) {
		load_fn_t fn = (load_fn_t)k_event_wait(event, 0xffffffff, !first, K_FOREVER);

		first = false;
		LOG_INF("fn %p ret %d", (void *)fn, fn());

		k_sem_give(sem);
	}
}

#define STACK_SIZE 4096

ZTEST(sof_boot, test_perf)
{
	/* Synchronization objects allocated on original uncached heap */
	struct k_event *u_event = k_object_alloc(K_OBJ_EVENT);
	struct k_event *k_event = k_object_alloc(K_OBJ_EVENT);

	zassert_not_null(u_event);
	zassert_not_null(k_event);

	k_event_init(u_event);
	k_event_init(k_event);

	struct k_sem *sem = k_object_alloc(K_OBJ_SEM);

	zassert_not_null(sem);
	k_sem_init(sem, 0, 1);

	/* Allocate kernel stack and thread and start it */
	struct k_thread *k_thread = k_object_alloc(K_OBJ_THREAD);

	zassert_not_null(k_thread);
	/* Important: Xtensa thread initialization code checks certain fields for 0 */
	memset(&k_thread->arch, 0, sizeof(k_thread->arch));

	k_thread_stack_t *k_stack = k_thread_stack_alloc(STACK_SIZE, 0);

	zassert_not_null(k_stack);

	struct k_thread *pk_thread = k_thread_create(k_thread, k_stack, STACK_SIZE, thread_fn,
						     k_event, sem, NULL, 0, 0, K_FOREVER);

	k_thread_start(pk_thread);

	/* Allocate userspace stack and thread and start it */
	struct k_thread *u_thread = k_object_alloc(K_OBJ_THREAD);

	zassert_not_null(u_thread);
	memset(&u_thread->arch, 0, sizeof(u_thread->arch));

	k_thread_stack_t *u_stack = k_thread_stack_alloc(STACK_SIZE, K_USER);

	zassert_not_null(u_stack);

	struct k_thread *pu_thread = k_thread_create(u_thread, u_stack, STACK_SIZE, thread_fn,
						     u_event, sem, NULL, 0, K_USER, K_FOREVER);

	zassert_not_null(pu_thread);
	k_thread_access_grant(pu_thread, u_event, sem);
	k_thread_start(pu_thread);

	for (unsigned int i = 0; i < ARRAY_SIZE(load_fn); i++) {
		LOG_INF("user: fn %p took %u", load_fn[i], test_perf(load_fn[i], u_event, sem));
		LOG_INF("kernel: fn %p took %u", load_fn[i], test_perf(load_fn[i], k_event, sem));
	}

	k_thread_abort(pu_thread);
	k_thread_stack_free(u_stack);
	k_thread_abort(pk_thread);
	k_thread_stack_free(k_stack);
	k_object_free(sem);
	k_object_free(u_event);
	k_object_free(k_event);
}
