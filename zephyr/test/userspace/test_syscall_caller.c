// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

/*
 * Negative tests for SOF system call validators: the caller restriction
 * (CONFIG_SOF_USERSPACE_SYSCALL_CALLER_CHECK), forged vregion handles and
 * DMA channel and direction checks.
 */

#include <sof/boot_test.h>
#include <sof/ipc/msg.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/lib/vregion.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/userspace/syscall_caller.h>
#include <rtos/userspace_helper.h>
#include <ipc4/handler.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	2048

static struct k_thread user_thread;
static K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);

/* kernel-only object whose address is passed as a forged handle */
static uint32_t kernel_only_word[16];

enum caller_test_case {
	/* allowed for any user thread */
	CALLER_ANY_CPU_GET_ID,
	/* restricted syscall from an unregistered thread */
	CALLER_UNREGISTERED_IPC_SYSCALL,
	/* registered as IPC thread, calling an IPC-only syscall */
	CALLER_IPC_IPC_SYSCALL,
	/* registered as IPC thread, calling an LL-only syscall */
	CALLER_IPC_LL_SYSCALL,
	/* registered, passing a kernel address as a vregion handle */
	CALLER_IPC_FORGED_VREGION,
	/* registered, starting a DMA channel number out of range */
	CALLER_IPC_DMA_BAD_CHANNEL,
	/* registered, DMA config direction not supported by the device */
	CALLER_IPC_DMA_BAD_DIRECTION,
};

#define HAVE_HDA_HOST_DMA \
	(CONFIG_SOF_USERSPACE_INTERFACE_DMA && CONFIG_DT_HAS_INTEL_ADSP_HDA_HOST_IN_ENABLED)

#if HAVE_HDA_HOST_DMA
/*
 * The DMA handle and channel are taken and released by the kernel side of
 * the test, so a user thread killed by the validator leaks nothing and the
 * suite can be run again.
 */
static struct sof_dma *test_dma;
static int test_channel;

static void dma_config_wrong_direction(struct sof_dma *dma, int channel)
{
	uint8_t buf[64] __aligned(64);
	struct dma_block_config blk = {
		.source_address = (uintptr_t)buf,
		.dest_address = (uintptr_t)buf,
		.block_size = sizeof(buf),
	};
	struct dma_config cfg = {
		/* the device only supports LMEM_TO_HMEM (MEMORY_TO_HOST) */
		.channel_direction = HOST_TO_MEMORY,
		.block_count = 1,
		.head_block = &blk,
	};

	(void)sof_dma_config(dma, channel, &cfg);
}
#endif

static void user_function(void *p1, void *p2, void *p3)
{
	enum caller_test_case test_case = (enum caller_test_case)(uintptr_t)p1;
#if HAVE_HDA_HOST_DMA
	struct sof_dma *dma = p2;
	int channel = (int)(uintptr_t)p3;
#endif

	zassert_true(k_is_user_context(), "isn't user");

	switch (test_case) {
	case CALLER_ANY_CPU_GET_ID:
		zassert_true(cpu_get_id() < CONFIG_CORE_COUNT);
		return;
	case CALLER_UNREGISTERED_IPC_SYSCALL:
		(void)ipc_wait_for_compound_msg();
		break;
	case CALLER_IPC_IPC_SYSCALL:
		zassert_equal(ipc_wait_for_compound_msg(), 0);
		return;
	case CALLER_IPC_LL_SYSCALL:
#if CONFIG_ZEPHYR_DP_SCHEDULER
		scheduler_dp_ll_tick();
		break;
#else
		ztest_test_skip();
		return;
#endif
	case CALLER_IPC_FORGED_VREGION:
#if CONFIG_SOF_USERSPACE_INTERFACE_VREGION
		(void)vregion_get((struct vregion *)kernel_only_word);
		break;
#else
		ztest_test_skip();
		return;
#endif
	case CALLER_IPC_DMA_BAD_CHANNEL:
#if HAVE_HDA_HOST_DMA
		(void)sof_dma_start(dma, 0x10000);
		break;
#else
		ztest_test_skip();
		return;
#endif
	case CALLER_IPC_DMA_BAD_DIRECTION:
#if HAVE_HDA_HOST_DMA
		dma_config_wrong_direction(dma, channel);
		break;
#else
		ztest_test_skip();
		return;
#endif
	default:
		zassert_unreachable("unknown test case");
	}

	zassert_unreachable("syscall security check did not fault");
}

static void run_case(enum caller_test_case test_case, bool register_ipc, bool expect_fault)
{
	void *dma = NULL;
	uintptr_t channel = 0;
	int ret;

#if HAVE_HDA_HOST_DMA
	if (test_case == CALLER_IPC_DMA_BAD_CHANNEL || test_case == CALLER_IPC_DMA_BAD_DIRECTION) {
		test_dma = sof_dma_get(SOF_DMA_DIR_LMEM_TO_HMEM, 0, SOF_DMA_DEV_HOST,
				       SOF_DMA_ACCESS_SHARED);
		zassert_not_null(test_dma, "sof_dma_get failed");
		test_channel = sof_dma_request_channel(test_dma, 0);
		zassert_true(test_channel >= 0, "sof_dma_request_channel failed: %d",
			     test_channel);
		dma = test_dma;
		channel = test_channel;
	}
#endif

	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			user_function, (void *)(uintptr_t)test_case, dma, (void *)channel,
			-1, K_USER, K_FOREVER);
	k_mem_domain_add_thread(zephyr_ll_mem_domain(), &user_thread);
#if HAVE_HDA_HOST_DMA
	user_grant_dma_access_all(&user_thread);
#endif

	/* IPC is disabled in standalone boot tests, so the IPC role is free */
	if (register_ipc)
		sof_sysuser_thread_set(SOF_SYSUSER_IPC, PLATFORM_PRIMARY_CORE_ID, &user_thread);
	if (expect_fault)
		sof_boot_test_set_fault_valid(&user_thread);

	k_thread_start(&user_thread);
	ret = k_thread_join(&user_thread, K_FOREVER);

	sof_sysuser_thread_set(SOF_SYSUSER_IPC, PLATFORM_PRIMARY_CORE_ID, NULL);
	sof_boot_test_set_fault_valid(NULL);

#if HAVE_HDA_HOST_DMA
	if (dma) {
		sof_dma_release_channel(test_dma, test_channel);
		sof_dma_put(test_dma);
		test_dma = NULL;
	}
#endif

	zassert_equal(ret, 0, "user thread join failed: %d", ret);
}

ZTEST(userspace_syscall_caller, cpu_get_id_any_thread)
{
	run_case(CALLER_ANY_CPU_GET_ID, false, false);
}

ZTEST(userspace_syscall_caller, unregistered_thread_rejected)
{
	run_case(CALLER_UNREGISTERED_IPC_SYSCALL, false, true);
}

ZTEST(userspace_syscall_caller, ipc_thread_allowed)
{
	run_case(CALLER_IPC_IPC_SYSCALL, true, false);
}

ZTEST(userspace_syscall_caller, ipc_thread_ll_syscall_rejected)
{
	run_case(CALLER_IPC_LL_SYSCALL, true, true);
}

ZTEST(userspace_syscall_caller, forged_vregion_rejected)
{
	run_case(CALLER_IPC_FORGED_VREGION, true, true);
}

ZTEST(userspace_syscall_caller, dma_bad_channel_rejected)
{
	run_case(CALLER_IPC_DMA_BAD_CHANNEL, true, true);
}

ZTEST(userspace_syscall_caller, dma_bad_direction_rejected)
{
	run_case(CALLER_IPC_DMA_BAD_DIRECTION, true, true);
}

ZTEST_SUITE(userspace_syscall_caller, sof_boot_test_standalone_predicate, NULL, NULL, NULL, NULL);

/**
 * SOF main has booted up and IPC handling is stopped.
 * Run test suites with ztest_run_all.
 */
static int run_tests(void)
{
	ztest_run_test_suite(userspace_syscall_caller, false, 1, 1, NULL);
	return 0;
}

SYS_INIT(run_tests, APPLICATION, 99);
