// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2025 Intel Corporation.
 */

/*
 * Test case for user-space use of the SOF DMA interface. The tests
 * transfer data from DSP to host using the host HD DMA instance.
 * The test uses the cavstool.py infrastructure to perform host side
 * programming of the HDA DMA, and to verify the transferred data.
 *
 * This test is based on the Zephyr kernel tests for Intel HD DMA
 * driver (zephyr/tests/boards/intel_adsp/hda/) written by Tom
 * Burdick. This test performs only subset of flows. Driver testing
 * should primarily done with the Zephyr kernel tests and this test
 * is solely to test the added syscall layer added in SOF.
 */

#include <sof/boot_test.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include <sof/lib/dma.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	2048
#define TEST_BUF_SIZE 256
#define TEST_CHANNEL 0
#define HD_DMA_BUF_ALIGN 128

static struct k_thread user_thread;
static K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);

K_SEM_DEFINE(ipc_sem_wake_user, 0, 1);
K_SEM_DEFINE(ipc_sem_wake_kernel, 0, 1);

static void intel_hda_dma_user(void *p1, void *p2, void *p3)
{
	uint8_t data_buf[TEST_BUF_SIZE] __aligned(HD_DMA_BUF_ALIGN);
	struct dma_block_config dma_block_cfg;
	struct dma_config config;
	struct dma_status stat;
	struct sof_dma *dma;
	uint32_t addr_align;
	int err, channel;

	zassert_true(k_is_user_context(), "isn't user");

	LOG_INF("SOF thread %s (%s)",
		k_is_user_context() ? "UserSpace!" : "privileged mode.",
		CONFIG_BOARD_TARGET);

	/*
	 * note: this gets a pointer to kernel memory this thread
	 * cannot access
	 */
	dma = sof_dma_get(SOF_DMA_DIR_LMEM_TO_HMEM, 0, SOF_DMA_DEV_HOST, SOF_DMA_ACCESS_SHARED);

	k_sem_take(&ipc_sem_wake_user, K_FOREVER);
	LOG_INF("configure DMA channel");

	channel = sof_dma_request_channel(dma, TEST_CHANNEL);
	zassert_equal(channel, TEST_CHANNEL);
	LOG_INF("sof_dma_request_channel: ret %d", channel);

	err = sof_dma_get_attribute(dma, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				    &addr_align);
	zassert_equal(err, 0);
	zassert_true(addr_align == HD_DMA_BUF_ALIGN);

	/* set up a DMA transfer */
	memset(&dma_block_cfg, 0, sizeof(dma_block_cfg));
	dma_block_cfg.dest_address = 0; /* host fifo */
	dma_block_cfg.source_address = (uintptr_t)data_buf;
	dma_block_cfg.block_size = sizeof(data_buf);

	/*
	 * fill data ramp, this payload is expected by host test
	 * harness
	 */
	for (uint32_t i = 0; i < TEST_BUF_SIZE; i++) {
		data_buf[i] = i & 0xff;
	}
	sys_cache_data_flush_range(data_buf, sizeof(data_buf));

	memset(&config, 0, sizeof(config));
	config.channel_direction = MEMORY_TO_HOST;
	config.block_count = 1;
	config.head_block = &dma_block_cfg;

	err = sof_dma_config(dma, channel, &config);
	zassert_equal(err, 0);
	LOG_INF("sof_dma_config: success");

	err = sof_dma_start(dma, channel);
	zassert_equal(err, 0);
	LOG_INF("sof_dma_start: ch %d", channel);

	k_sem_give(&ipc_sem_wake_kernel);
	LOG_INF("setup ready, waiting for kernel to configure host-side of the test");
	k_sem_take(&ipc_sem_wake_user, K_FOREVER);
	LOG_INF("start DMA test and transfer data");

	err = sof_dma_get_status(dma, channel, &stat);
	zassert_equal(err, 0);
	LOG_INF("sof_dma_get_status start: pend %u free %u",
		stat.pending_length, stat.free);

	err = sof_dma_reload(dma, channel, sizeof(data_buf));
	zassert_equal(err, 0);

	for (int i = 0; stat.pending_length < TEST_BUF_SIZE; i++) {
		err = sof_dma_get_status(dma, channel, &stat);
		zassert_equal(err, 0);
		LOG_INF("sof_dma_get_status %d: pend %u free %u", i,
			stat.pending_length, stat.free);

		zassert_true(i < 100, "DMA transfer completes in 100usec");

		/* let DMA transfer complete */
		k_sleep(K_USEC(1));
	}

	err = sof_dma_get_status(dma, channel, &stat);
	zassert_equal(err, 0);
	LOG_INF("sof_dma_get_status end: pend %u free %u",
		stat.pending_length, stat.free);

	LOG_INF("transfer done, asking host to validate output");
	k_sem_give(&ipc_sem_wake_kernel);
	k_sem_take(&ipc_sem_wake_user, K_FOREVER);
	LOG_INF("test done, cleaning up resources");

	err = sof_dma_stop(dma, channel);
	zassert_equal(err, 0);

	sof_dma_release_channel(dma, channel);

	sof_dma_put(dma);

	LOG_INF("DMA stopped and resources freed");

	k_sem_give(&ipc_sem_wake_kernel);
}

#define IPC_TIMEOUT K_MSEC(1500)
#define DMA_BUF_SIZE 256

#define ALIGNMENT DMA_BUF_ADDR_ALIGNMENT(DT_NODELABEL(hda_host_in))
static __aligned(ALIGNMENT) uint8_t dma_buf[DMA_BUF_SIZE];

#include <intel_adsp_hda.h>
#include <../../../../zephyr/tests/boards/intel_adsp/hda/src/tests.h>

static int msg_validate_res;

static bool ipc_message(const struct device *dev, void *arg,
			uint32_t data, uint32_t ext_data)
{
	LOG_DBG("HDA message received, data %u, ext_data %u", data, ext_data);
	msg_validate_res = ext_data;
	return true;
}

static void intel_hda_dma_kernel(void)
{
	const struct device *dma;

	LOG_INF("run %s with buffer at address %p, size %d",
		__func__, (void *)dma_buf, DMA_BUF_SIZE);

	intel_adsp_ipc_set_message_handler(INTEL_ADSP_IPC_HOST_DEV, ipc_message, NULL);

	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			intel_hda_dma_user, NULL, NULL, NULL,
			-1, K_USER, K_FOREVER);

	k_thread_access_grant(&user_thread, &ipc_sem_wake_user);
	k_thread_access_grant(&user_thread, &ipc_sem_wake_kernel);

	dma = DEVICE_DT_GET(DT_NODELABEL(hda_host_in));
	k_thread_access_grant(&user_thread, dma);

	hda_ipc_msg(INTEL_ADSP_IPC_HOST_DEV, IPCCMD_HDA_RESET,
		    TEST_CHANNEL, IPC_TIMEOUT);

	hda_ipc_msg(INTEL_ADSP_IPC_HOST_DEV, IPCCMD_HDA_CONFIG,
		    TEST_CHANNEL | (DMA_BUF_SIZE << 8), IPC_TIMEOUT);

	k_thread_start(&user_thread);

	LOG_INF("user started, waiting for it to be ready");

	k_sem_give(&ipc_sem_wake_user);
	k_sem_take(&ipc_sem_wake_kernel, K_FOREVER);

	LOG_INF("user ready, starting HDA test");

	hda_ipc_msg(INTEL_ADSP_IPC_HOST_DEV, IPCCMD_HDA_START, TEST_CHANNEL, IPC_TIMEOUT);

	k_sem_give(&ipc_sem_wake_user);
	k_sem_take(&ipc_sem_wake_kernel, K_FOREVER);

	LOG_INF("transfer done, validating results");

	hda_ipc_msg(INTEL_ADSP_IPC_HOST_DEV, IPCCMD_HDA_VALIDATE, TEST_CHANNEL,
		    IPC_TIMEOUT);

	hda_dump_regs(HOST_OUT, HDA_REGBLOCK_SIZE, TEST_CHANNEL, "host reset");

	k_sem_give(&ipc_sem_wake_user);
	k_sem_take(&ipc_sem_wake_kernel, K_FOREVER);

	LOG_INF("test done, terminate user thread");

	k_thread_join(&user_thread, K_FOREVER);

	zassert_true(msg_validate_res == 1, "DMA transferred data invalid payload");
}

ZTEST(userspace_intel_hda_dma, dma_mem_to_host)
{
	intel_hda_dma_kernel();

	ztest_test_pass();
}

ZTEST_SUITE(userspace_intel_hda_dma, NULL, NULL, NULL, NULL, NULL);

/**
 * SOF main has booted up and IPC handling is stopped.
 * Run test suites with ztest_run_all.
 */
static int run_tests(void)
{
	ztest_run_test_suite(userspace_intel_hda_dma, false, 1, 1, NULL);
	return 0;
}

SYS_INIT(run_tests, APPLICATION, 99);
