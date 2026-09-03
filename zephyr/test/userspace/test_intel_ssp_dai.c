// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2025 Intel Corporation.
 */

/*
 * Test case for user-space use of the SOF DMA interface. The tests
 * covers all key interfaces of DMA and DAI, testing their use from
 * a user-space threads. Due to hardware constraints, the actual DMA
 * transfers cannot be tested as this would require cooperation with a
 * host entity that would manage the HDA link DMA in sync with the DP
 * test case. Test does check all programming can be done and no
 * errors are raised from the drivers. Valid configuration blobs are
 * passed, to fully exercise the drivers interfaces.

 * Requirements for host side test execution environment:
 *  - I2S offload must be enabled on host side (HDAMLI2S) to allow
 *    the DAI driver to access hardware registers.
 */

#include <sof/boot_test.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include <sof/lib/dai.h>
#include <sof/lib/uuid.h>
#include <sof/lib/dma.h>
#include <sof/audio/component_ext.h>

#include "../../../src/audio/copier/dai_copier.h"
#include "../../../../zephyr/drivers/dai/intel/ssp/ssp.h"

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	8192
#define HD_DMA_BUF_ALIGN 128
#define TEST_BUF_SIZE (2*HD_DMA_BUF_ALIGN)
#define TEST_CHANNEL_OUT 3
#define TEST_CHANNEL_IN 4
#define SSP_DEVICE ssp00

static struct k_thread user_thread;
static K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);

static K_SEM_DEFINE(ipc_sem_wake_user, 0, 1);
static K_SEM_DEFINE(ipc_sem_wake_kernel, 0, 1);

static int call_dai_set_ssp_v3_config_48k_2ch_32bit(const struct device *dai_dev)
{
	union hdalink_cfg link_cfg;
	const uint8_t stream_id = 0;

	link_cfg.full = 0;
	link_cfg.part.dir = DAI_DIR_TX;
	link_cfg.part.stream = stream_id;

	struct dai_config common_config = {
		.type = DAI_INTEL_SSP_NHLT,
		.dai_index = 0,
		.channels = 2,
		.rate = 48000,
		.format = DAI_CBC_CFC | DAI_PROTO_I2S | DAI_INVERSION_NB_NF,
		.options = 0,
		.word_size = 32,
		.block_size = 0,
		.link_config = link_cfg.full,
		.tdm_slot_group = 0
	};

	/*
	 * There are no suitable struct definitions to create these
	 * config objects, so we have to define a custom type that
	 * includes the common header, a single MDIV entry, one TLV
	 * entry and the link_ctl struct. These are normally part of
	 * ACPI NHLT and can be alternatively created with alsa-utils
	 * nhlt plugin.
	 */
	struct {
		struct dai_intel_ipc4_ssp_configuration_blob_ver_3_0 b;
		uint32_t mdivr0;
		uint32_t type;
		uint32_t size;
		struct ssp_intel_link_ctl link_ctl;
	} __packed blob30;

	memset(&blob30, 0, sizeof(blob30));
	/* DAI config blob header for SSP v3 */
	blob30.b.version =			SSP_BLOB_VER_3_0;
	blob30.b.size =			sizeof(blob30);
	/* I2S config matching sof-ptl-nocodec.tplg (32bit/48kHz/2ch) */
	blob30.b.i2s_ssp_config.ssc0 =		0x81d0077f;
	blob30.b.i2s_ssp_config.ssc1 =		0xd0400004;
	blob30.b.i2s_ssp_config.sscto =	0;
	blob30.b.i2s_ssp_config.sspsp =	0x02200000;
	blob30.b.i2s_ssp_config.ssc2 =		0x00004002;
	blob30.b.i2s_ssp_config.sspsp2 =	0;
	blob30.b.i2s_ssp_config.ssc3 =		0;
	blob30.b.i2s_ssp_config.ssioc =	0x00000020;
	/* clock control settings matching sof-ptl-nocodec.tplg */
	blob30.b.i2s_mclk_control.mdivctlr =	0x00010001;
	blob30.b.i2s_mclk_control.mdivrcnt =	1;
	/* variable-size section of clock control, one entry for mdivr */
	blob30.mdivr0 = 0xfff;
	/* aux-data with one TLV entry for link-clk-source */
	blob30.type = SSP_LINK_CLK_SOURCE;
	blob30.size = sizeof(struct ssp_intel_link_ctl);
	blob30.link_ctl.clock_source = 1;

	return dai_config_set(dai_dev, &common_config, &blob30, sizeof(blob30));
}

static void intel_ssp_dai_user(void *p1, void *p2, void *p3)
{
	struct dma_block_config dma_block_cfg;
	struct sof_dma *dma_in, *dma_out;
	uint8_t data_buf_out[TEST_BUF_SIZE] __aligned(HD_DMA_BUF_ALIGN);
	uint8_t data_buf_in[TEST_BUF_SIZE] __aligned(HD_DMA_BUF_ALIGN);
	uint32_t addr_align = 0;
	const struct device *dai_dev;
	struct dai_properties dai_props;
	struct dma_config config;
	struct dma_status stat;
	int err, channel_out, channel_in;

	zassert_true(k_is_user_context());

	/*
	 * note: this gets a pointer to kernel memory this thread
	 * cannot access
	 */
	dma_in = sof_dma_get(SOF_DMA_DIR_DEV_TO_MEM, 0, SOF_DMA_DEV_SSP, SOF_DMA_ACCESS_SHARED);
	dma_out = sof_dma_get(SOF_DMA_DIR_MEM_TO_DEV, 0, SOF_DMA_DEV_SSP, SOF_DMA_ACCESS_SHARED);

	k_sem_take(&ipc_sem_wake_user, K_FOREVER);

	LOG_INF("create a DAI device for %s", STRINGIFY(SSP_DEVICE));

	dai_dev = DEVICE_DT_GET(DT_NODELABEL(SSP_DEVICE));
	err = dai_probe(dai_dev);
	zassert_equal(err, 0);

	channel_out = sof_dma_request_channel(dma_out, TEST_CHANNEL_OUT);
	zassert_equal(channel_out, TEST_CHANNEL_OUT);
	LOG_INF("sof_dma_request_channel (out): ret ch %d", channel_out);
	channel_in = sof_dma_request_channel(dma_in, TEST_CHANNEL_IN);
	zassert_equal(channel_in, TEST_CHANNEL_IN);
	LOG_INF("sof_dma_request_channel (in): ret ch %d", channel_in);

	err = sof_dma_get_attribute(dma_out, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				    &addr_align);
	zassert_equal(err, 0);
	zassert_true(addr_align == HD_DMA_BUF_ALIGN);

	/* set up a DMA transfer */
	memset(&dma_block_cfg, 0, sizeof(dma_block_cfg));

	err = dai_get_properties_copy(dai_dev, DAI_DIR_TX, 0, &dai_props);
	zassert_equal(err, 0);

	LOG_INF("dai_get_properties_copy (TX), ret %d, fifo %u", err, dai_props.fifo_address);

	dma_block_cfg.dest_address = dai_props.fifo_address; /* dai fifo */
	dma_block_cfg.source_address = (uintptr_t)data_buf_out;
	dma_block_cfg.block_size = sizeof(data_buf_out);

	memset(&config, 0, sizeof(config));
	config.channel_direction = MEMORY_TO_PERIPHERAL;
	config.block_count = 1;
	config.head_block = &dma_block_cfg;
	config.source_data_size = 4;
	config.dest_data_size = 4;

	err = sof_dma_config(dma_out, channel_out, &config);
	zassert_equal(err, 0);

	err = dai_get_properties_copy(dai_dev, DAI_DIR_RX, 0, &dai_props);
	zassert_equal(err, 0);
	LOG_INF("dai_get_properties_copy (RX), ret %d, fifo %u", err, dai_props.fifo_address);

	dma_block_cfg.dest_address = (uintptr_t)data_buf_in;
	dma_block_cfg.source_address = dai_props.fifo_address; /* dai fifo */
	dma_block_cfg.block_size = sizeof(data_buf_in);

	config.channel_direction = PERIPHERAL_TO_MEMORY;
	config.block_count = 1;

	err = sof_dma_config(dma_in, channel_in, &config);
	zassert_equal(err, 0, "dma-config error");

	err = call_dai_set_ssp_v3_config_48k_2ch_32bit(dai_dev);
	zassert_equal(err, 0);
	LOG_INF("DAI configuration ready, sync with kernel on start");

	k_sem_give(&ipc_sem_wake_kernel);
	k_sem_take(&ipc_sem_wake_user, K_FOREVER);
	LOG_INF("start DMA test and transfer data");

	err = dai_trigger(dai_dev, DAI_DIR_RX, DAI_TRIGGER_PRE_START);
	zassert_equal(err, 0);

	err = dai_trigger(dai_dev, DAI_DIR_TX, DAI_TRIGGER_PRE_START);
	zassert_equal(err, 0);
	LOG_INF("dai_trigger RX+TX PRE_START done");

	err = sof_dma_get_status(dma_in, channel_in, &stat);
	zassert_equal(err, 0);
	LOG_INF("sof_dma_get_status ( dma_in/start):\tpend %3u free %3u",
		stat.pending_length, stat.free);

	err = sof_dma_get_status(dma_out, channel_out, &stat);
	zassert_equal(err, 0);
	LOG_INF("sof_dma_get_status (dma_out/start):\tpend %3u free %3u",
		stat.pending_length, stat.free);

	err = sof_dma_start(dma_in, channel_in);
	zassert_equal(err, 0);

	err = sof_dma_start(dma_out, channel_out);
	zassert_equal(err, 0);

	err = dai_trigger(dai_dev, DAI_DIR_RX, DAI_TRIGGER_START);
	zassert_equal(err, 0);

	err = dai_trigger(dai_dev, DAI_DIR_TX, DAI_TRIGGER_START);
	zassert_equal(err, 0);
	LOG_INF("DMAs and DAIs started.");

	k_sleep(K_USEC(10));

	err = sof_dma_get_status(dma_in, channel_in, &stat);
	zassert_equal(err, 0);
	/* after start, there should be at least some free data */
	zassert_true(stat.free > 0);
	zassert_true(stat.pending_length < TEST_BUF_SIZE);
	LOG_INF("sof_dma_get_status ( dma_in/run):\tpend %3u free %3u",
		stat.pending_length, stat.free);

	err = sof_dma_reload(dma_in, channel_in, sizeof(data_buf_in));
	zassert_equal(err, 0);

	err = sof_dma_get_status(dma_in, channel_in, &stat);
	zassert_equal(err, 0);
	/* after reload, there should be at least some data pending */
	zassert_true(stat.free < TEST_BUF_SIZE);
	zassert_true(stat.pending_length > 0);

	err = sof_dma_get_status(dma_out, channel_out, &stat);
	zassert_equal(err, 0);
	LOG_INF("sof_dma_get_status (dma_out/run):\tpend %3u free %3u",
		stat.pending_length, stat.free);
	zassert_true(stat.free < TEST_BUF_SIZE);
	zassert_true(stat.pending_length > 0);

	LOG_INF("DMA setup done, asking host to clean up ");
	k_sem_give(&ipc_sem_wake_kernel);
	k_sem_take(&ipc_sem_wake_user, K_FOREVER);
	LOG_INF("Cleaning up resources");

	err = sof_dma_stop(dma_out, channel_out);
	zassert_equal(err, 0);

	err = sof_dma_stop(dma_in, channel_in);
	zassert_equal(err, 0);

	err = dai_trigger(dai_dev, DAI_DIR_TX, DAI_TRIGGER_STOP);
	zassert_equal(err, 0);

	err = dai_trigger(dai_dev, DAI_DIR_RX, DAI_TRIGGER_STOP);
	zassert_equal(err, 0);

	sof_dma_release_channel(dma_out, channel_out);

	sof_dma_release_channel(dma_in, channel_in);

	err = dai_remove(dai_dev);
	zassert_equal(err, 0);

	sof_dma_put(dma_in);
	sof_dma_put(dma_out);

	LOG_INF("Cleanup successful, terminating user thread.");

	k_sem_give(&ipc_sem_wake_kernel);
}

static void intel_ssp_dai_kernel(void)
{
	const struct device *dma_out, *dma_in;
	const struct device *dai_dev;

	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			intel_ssp_dai_user, NULL, NULL, NULL,
			-1, K_USER, K_FOREVER);

	k_thread_access_grant(&user_thread, &ipc_sem_wake_user);
	k_thread_access_grant(&user_thread, &ipc_sem_wake_kernel);

	dma_out = DEVICE_DT_GET(DT_NODELABEL(hda_link_out));
	dma_in = DEVICE_DT_GET(DT_NODELABEL(hda_link_in));
	dai_dev = DEVICE_DT_GET(DT_NODELABEL(SSP_DEVICE));

	k_thread_access_grant(&user_thread, dma_out);
	k_thread_access_grant(&user_thread, dma_in);
	k_thread_access_grant(&user_thread, dai_dev);

	k_thread_start(&user_thread);

	LOG_INF("user started, waiting for it to be ready");

	k_sem_give(&ipc_sem_wake_user);
	k_sem_take(&ipc_sem_wake_kernel, K_FOREVER);

	LOG_INF("user ready, starting HDA test");

	k_sem_give(&ipc_sem_wake_user);
	k_sem_take(&ipc_sem_wake_kernel, K_FOREVER);

	LOG_INF("transfer done, grant permission to clean up");

	k_sem_give(&ipc_sem_wake_user);
	k_sem_take(&ipc_sem_wake_kernel, K_FOREVER);

	LOG_INF("test done, terminate user thread");

	k_thread_join(&user_thread, K_FOREVER);
}

ZTEST(userspace_intel_dai_ssp, dai_ssp_loopback_setup)
{
	intel_ssp_dai_kernel();

	ztest_test_pass();
}

ZTEST_SUITE(userspace_intel_dai_ssp, NULL, NULL, NULL, NULL, NULL);

/**
 * SOF main has booted up and IPC handling is stopped.
 * Run test suites with ztest_run_all.
 */
static int run_tests(void)
{
	ztest_run_test_suite(userspace_intel_dai_ssp, false, 1, 1, NULL);
	return 0;
}

SYS_INIT(run_tests, APPLICATION, 99);
