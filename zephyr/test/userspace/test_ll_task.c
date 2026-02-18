// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

/*
 * Test case for creation of low-latency threads in user-space.
 */

#include <sof/boot_test.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/uuid.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/buffer.h>
#include <sof/ipc/common.h>
#include <sof/ipc/topology.h>
#include <rtos/task.h>
#include <rtos/userspace_helper.h>
#include <ipc4/fw_reg.h>
#include <ipc4/module.h>
#include <ipc4/gateway.h>
#include <ipc4/header.h>
#include <ipc4/base_fw_vendor.h>
#include <module/ipc4/base-config.h>
#include <rimage/sof/user/manifest.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include <zephyr/app_memory/app_memdomain.h>

#include <stddef.h> /* offsetof() */
#include <string.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

/* f11818eb-e92e-4082-82a3-dc54c604ebf3 */
SOF_DEFINE_UUID("test_task", test_task_uuid, 0xf11818eb, 0xe92e, 0x4082,
		0x82,  0xa3, 0xdc, 0x54, 0xc6, 0x04, 0xeb, 0xf3);

K_APPMEM_PARTITION_DEFINE(userspace_ll_part);

/* Global variable for test runs counter, accessible from user-space */
K_APP_BMEM(userspace_ll_part) static int test_runs;

/* User-space thread for pipeline_two_components test */
#define PPL_USER_STACKSIZE 4096

static struct k_thread ppl_user_thread;
static K_THREAD_STACK_DEFINE(ppl_user_stack, PPL_USER_STACKSIZE);

static enum task_state task_callback(void *arg)
{
	LOG_INF("entry");

	if (++test_runs > 3)
		return SOF_TASK_STATE_COMPLETED;

	return SOF_TASK_STATE_RESCHEDULE;
}

static void ll_task_test(void)
{
	struct task *task;
	int priority = 0;
	int core = 0;
	int ret;

	/* Initialize global test runs counter */
	test_runs = 0;

	task = zephyr_ll_task_alloc();
	zassert_not_null(task, "task allocation failed");

	/* allow user space to report status via 'test_runs' */
	k_mem_domain_add_partition(zephyr_ll_mem_domain(), &userspace_ll_part);

	/* work in progress, see pipeline-schedule.c */
	ret = schedule_task_init_ll(task, SOF_UUID(test_task_uuid), SOF_SCHEDULE_LL_TIMER,
				    priority, task_callback,
				    (void *)&test_runs, core, 0);
	zassert_equal(ret, 0);

	LOG_INF("task init done");

	/* Schedule the task to run immediately with 1ms period */
	ret = schedule_task(task, 0, 1000);  /* 0 = start now, 1000us = 1ms period */
	zassert_equal(ret, 0);

	LOG_INF("task scheduled and running");

	/* Let the task run for a bit */
	k_sleep(K_MSEC(10));

	/* Cancel the task to stop any scheduled execution */
	ret = schedule_task_cancel(task);
	zassert_equal(ret, 0);

	/* Free task resources */
	ret = schedule_task_free(task);
	zassert_equal(ret, 0);

	LOG_INF("test complete");
}

ZTEST(userspace_ll, ll_task_test)
{
	ll_task_test();
}

static void pipeline_check(void)
{
	struct pipeline *p;
	struct k_heap *heap;
	uint32_t pipeline_id = 1;
	uint32_t priority = 5;
	uint32_t comp_id = 10;
	int ret;

	heap = zephyr_ll_user_heap();
	zassert_not_null(heap, "user heap not found");

	/* Create pipeline on user heap */
	p = pipeline_new(heap, pipeline_id, priority, comp_id, NULL);
	zassert_not_null(p, "pipeline creation failed");

	/* Verify heap assignment */
	zassert_equal(p->heap, heap, "pipeline heap not equal to user heap");

	/* Verify pipeline properties */
	zassert_equal(p->pipeline_id, pipeline_id, "pipeline id mismatch");
	zassert_equal(p->priority, priority, "priority mismatch");
	zassert_equal(p->comp_id, comp_id, "comp id mismatch");

	/* Free pipeline */
	ret = pipeline_free(p);
	zassert_ok(ret, "pipeline free failed");
}

ZTEST(userspace_ll, pipeline_check)
{
	pipeline_check();
}

/* Copier UUID: 9ba00c83-ca12-4a83-943c-1fa2e82f9dda */
static const uint8_t copier_uuid[16] = {
	0x83, 0x0c, 0xa0, 0x9b, 0x12, 0xca, 0x83, 0x4a,
	0x94, 0x3c, 0x1f, 0xa2, 0xe8, 0x2f, 0x9d, 0xda
};

/**
 * Find the module_id (manifest entry index) for the copier module
 * by iterating the firmware manifest and matching the copier UUID.
 */
static int find_copier_module_id(void)
{
	const struct sof_man_fw_desc *desc = basefw_vendor_get_manifest();
	const struct sof_man_module *mod;
	uint32_t i;

	if (!desc)
		return -1;

	for (i = 0; i < desc->header.num_module_entries; i++) {
		mod = (const struct sof_man_module *)((const char *)desc +
						      SOF_MAN_MODULE_OFFSET(i));
		if (!memcmp(&mod->uuid, copier_uuid, sizeof(copier_uuid)))
			return (int)i;
	}

	return -1;
}

/**
 * IPC4 copier module config - used as payload for comp_new_ipc4().
 * Placed at MAILBOX_HOSTBOX_BASE before calling comp_new_ipc4().
 * Layout matches struct ipc4_copier_module_cfg from copier.h.
 */
struct copier_init_data {
	struct ipc4_base_module_cfg base;
	struct ipc4_audio_format out_fmt;
	uint32_t copier_feature_mask;
	/* Gateway config (matches struct ipc4_copier_gateway_cfg) */
	union ipc4_connector_node_id node_id;
	uint32_t dma_buffer_size;
	uint32_t config_length;
} __packed __aligned(4);

static void fill_audio_format(struct ipc4_audio_format *fmt)
{
	memset(fmt, 0, sizeof(*fmt));
	fmt->sampling_frequency = IPC4_FS_48000HZ;
	fmt->depth = IPC4_DEPTH_32BIT;
	fmt->ch_cfg = IPC4_CHANNEL_CONFIG_STEREO;
	fmt->channels_count = 2;
	fmt->valid_bit_depth = 32;
	fmt->s_type = IPC4_TYPE_MSB_INTEGER;
	fmt->interleaving_style = IPC4_CHANNELS_INTERLEAVED;
}

/**
 * Create a copier component via IPC4.
 *
 * @param module_id   Copier module_id from manifest
 * @param instance_id Instance ID for this component
 * @param pipeline_id Parent pipeline ID
 * @param node_id     Gateway node ID (type + virtual DMA index)
 */
static struct comp_dev *create_copier(int module_id, int instance_id,
				      int pipeline_id,
				      union ipc4_connector_node_id node_id)
{
	struct ipc4_module_init_instance module_init;
	struct copier_init_data cfg;
	struct comp_dev *dev;

	/* Prepare copier config payload */
	memset(&cfg, 0, sizeof(cfg));
	fill_audio_format(&cfg.base.audio_fmt);
	/* 2 channels * 4 bytes * 48 frames = 384 bytes */
	cfg.base.ibs = 384;
	cfg.base.obs = 384;
	cfg.base.is_pages = 0;
	cfg.base.cpc = 0;
	cfg.out_fmt = cfg.base.audio_fmt;
	cfg.copier_feature_mask = 0;
	cfg.node_id = node_id;
	cfg.dma_buffer_size = 768;
	cfg.config_length = 0;

	/* Write config data to mailbox hostbox (where comp_new_ipc4 reads it).
	 * Flush cache so that data is visible in SRAM before comp_new_ipc4()
	 * invalidates the cache line (in normal IPC flow, host writes via DMA
	 * directly to SRAM, so the invalidation reads fresh data; here the DSP
	 * core itself writes, so an explicit flush is needed).
	 */
	memcpy((void *)MAILBOX_HOSTBOX_BASE, &cfg, sizeof(cfg));
	sys_cache_data_flush_range((void *)MAILBOX_HOSTBOX_BASE, sizeof(cfg));

	/* Prepare IPC4 module init header */
	memset(&module_init, 0, sizeof(module_init));
	module_init.primary.r.module_id = module_id;
	module_init.primary.r.instance_id = instance_id;
	module_init.primary.r.type = SOF_IPC4_MOD_INIT_INSTANCE;
	module_init.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
	module_init.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;

	module_init.extension.r.param_block_size = sizeof(cfg) / sizeof(uint32_t);
	module_init.extension.r.ppl_instance_id = pipeline_id;
	module_init.extension.r.core_id = 0;
	module_init.extension.r.proc_domain = 0; /* LL */

	dev = comp_new_ipc4(&module_init);

	/*
	 * We use the IPC code to create the components. This code runs
	 * in kernel space, so we need to separately assign thecreated
	 * components to the user LL and IPC threads before it can be used.
	 */
	comp_grant_access_to_thread(dev, zephyr_domain_thread_tid(zephyr_ll_domain()));
	comp_grant_access_to_thread(dev, &ppl_user_thread);

	return dev;
}

/**
 * Context shared between kernel setup and the user-space pipeline thread.
 */
struct ppl_test_ctx {
	struct pipeline *p;
	struct k_heap *heap;
	struct comp_dev *host_comp;
	struct comp_dev *dai_comp;
	struct comp_buffer *buf;
	struct ipc *ipc;
	struct ipc_comp_dev *ipc_pipe;
};

/**
 * Pipeline operations: connect, complete, prepare, copy, verify, and clean up.
 * This function is called either directly (kernel mode) or from a user-space
 * thread, exercising pipeline_*() calls from the requested context.
 */
static void pipeline_ops(struct ppl_test_ctx *ctx)
{
	struct pipeline *p = ctx->p;
	struct comp_dev *host_comp = ctx->host_comp;
	struct comp_dev *dai_comp = ctx->dai_comp;
	struct comp_buffer *buf = ctx->buf;
	int ret;

	LOG_INF("pipeline_ops: user_context=%d", k_is_user_context());

	/* Step 6: Connect host -> buffer -> DAI */
	ret = pipeline_connect(host_comp, buf, PPL_CONN_DIR_COMP_TO_BUFFER);
	zassert_equal(ret, 0, "connect host to buffer failed");

	ret = pipeline_connect(dai_comp, buf, PPL_CONN_DIR_BUFFER_TO_COMP);
	zassert_equal(ret, 0, "connect buffer to DAI failed");

	LOG_INF("host -> buffer -> DAI connected");

	/* Step 7: Complete the pipeline */
	ret = pipeline_complete(p, host_comp, dai_comp);
	zassert_equal(ret, 0, "pipeline complete failed");

	/* Step 8: Prepare the pipeline */
	p->sched_comp = host_comp;
	k_sleep(K_MSEC(10));

	ret = pipeline_prepare(p, host_comp);
	zassert_equal(ret, 0, "pipeline prepare failed");

	LOG_INF("pipeline complete, status = %d", p->status);

	/* Step 9: Run copies */
	pipeline_copy(p);
	pipeline_copy(p);

	/* Verify pipeline source and sink assignments */
	zassert_equal(p->source_comp, host_comp, "source comp mismatch");
	zassert_equal(p->sink_comp, dai_comp, "sink comp mismatch");

	LOG_INF("pipeline_ops done");
}

/**
 * User-space thread entry point for pipeline_two_components test.
 * p1 points to the ppl_test_ctx shared with the kernel launcher.
 */
static void pipeline_user_thread(void *p1, void *p2, void *p3)
{
	struct ppl_test_ctx *ctx = (struct ppl_test_ctx *)p1;

	zassert_true(k_is_user_context(), "expected user context");
	pipeline_ops(ctx);
}

/**
 * Test creating a pipeline with a host copier and a DAI (link) copier,
 * connected through a shared buffer.
 *
 * When run_in_user is true, all pipeline_*() calls are made from a
 * separate user-space thread.
 */
static void pipeline_two_components(bool run_in_user)
{
	struct ppl_test_ctx *ctx;
	struct k_heap *heap = NULL;
	uint32_t pipeline_id = 2;
	uint32_t priority = 0;
	uint32_t comp_id;
	int copier_module_id;
	int host_instance_id = 0;
	int dai_instance_id = 1;
	int ret;

	/* Step: Find the copier module_id from the firmware manifest */
	copier_module_id = find_copier_module_id();
	zassert_true(copier_module_id >= 0, "copier module not found in manifest");
	LOG_INF("copier module_id = %d", copier_module_id);

	/* Step: Create pipeline */
	if (run_in_user) {
		LOG_INF("running test with user memory domain");
		heap = zephyr_ll_user_heap();
		zassert_not_null(heap, "user heap not found");
	} else {
		LOG_INF("running test with kernel  memory domain");
	}

	ctx = sof_heap_alloc(heap, SOF_MEM_FLAG_USER, sizeof(*ctx), 0);
	ctx->heap = heap;
	ctx->ipc = ipc_get();

	comp_id = IPC4_COMP_ID(copier_module_id, host_instance_id);
	ctx->p = pipeline_new(ctx->heap, pipeline_id, priority, comp_id, NULL);
	zassert_not_null(ctx->p, "pipeline creation failed");

	/* Set pipeline period so components get correct dev->period and dev->frames.
	 * This mirrors what ipc4_create_pipeline() does in normal IPC flow.
	 */
	ctx->p->time_domain = SOF_TIME_DOMAIN_TIMER;
	ctx->p->period = LL_TIMER_PERIOD_US;

	/* Register pipeline in IPC component list so comp_new_ipc4() can
	 * find it via ipc_get_comp_by_ppl_id() and set dev->period.
	 */
	ctx->ipc_pipe = rzalloc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
			       sizeof(struct ipc_comp_dev));
	zassert_not_null(ctx->ipc_pipe, "ipc_comp_dev alloc failed");
	ctx->ipc_pipe->pipeline = ctx->p;
	ctx->ipc_pipe->type = COMP_TYPE_PIPELINE;
	ctx->ipc_pipe->id = pipeline_id;
	ctx->ipc_pipe->core = 0;
	list_item_append(&ctx->ipc_pipe->list, &ctx->ipc->comp_list);

	/* Step: Create host copier with HDA host output gateway */
	union ipc4_connector_node_id host_node_id = { .f = {
		.dma_type = ipc4_hda_host_output_class,
		.v_index = 0
	}};
	ctx->host_comp = create_copier(copier_module_id, host_instance_id, pipeline_id,
				      host_node_id);
	zassert_not_null(ctx->host_comp, "host copier creation failed");

	/* Assign pipeline to host component */
	ctx->host_comp->pipeline = ctx->p;
	ctx->host_comp->ipc_config.type = SOF_COMP_HOST;

	LOG_INF("host copier created, comp_id = 0x%x", ctx->host_comp->ipc_config.id);

	/* Step: Create link copier with HDA link output gateway */
	union ipc4_connector_node_id link_node_id = { .f = {
		.dma_type = ipc4_hda_link_output_class,
		.v_index = 0
	}};
	ctx->dai_comp = create_copier(copier_module_id, dai_instance_id, pipeline_id,
				     link_node_id);
	zassert_not_null(ctx->dai_comp, "DAI copier creation failed");

	/* Assign pipeline to DAI component */
	ctx->dai_comp->pipeline = ctx->p;
	ctx->dai_comp->ipc_config.type = SOF_COMP_DAI;

	LOG_INF("DAI copier created, comp_id = 0x%x", ctx->dai_comp->ipc_config.id);

	/* Step: Allocate a buffer to connect host -> DAI */
	ctx->buf = buffer_alloc(ctx->heap, 384, 0, 0, false);
	zassert_not_null(ctx->buf, "buffer allocation failed");

	if (run_in_user) {
		/* Create a user-space thread to execute pipeline operations */
		k_thread_create(&ppl_user_thread, ppl_user_stack, PPL_USER_STACKSIZE,
				pipeline_user_thread, ctx, NULL, NULL,
				-1, K_USER, K_FOREVER);

		/* Add thread to LL memory domain so it can access pipeline memory */
		k_mem_domain_add_thread(zephyr_ll_mem_domain(), &ppl_user_thread);

		user_grant_dai_access_all(&ppl_user_thread);
		user_grant_dma_access_all(&ppl_user_thread);
		user_access_to_mailbox(zephyr_ll_mem_domain(), &ppl_user_thread);

		/*
		 * A hack for testing purposes, normally DAI module
		 * is created in user-space so it gets access
		 * automatically. Until that works, use dai_dd directly.
		 */
		struct dai_data *dai_dd = comp_get_drvdata(ctx->dai_comp);
		struct k_mutex *dai_lock = dai_dd->dai->lock;
		k_thread_access_grant(&ppl_user_thread, dai_lock);

		k_thread_start(&ppl_user_thread);

		LOG_INF("user thread started, waiting for completion");

		k_thread_join(&ppl_user_thread, K_FOREVER);
	} else {
		/* Run pipeline operations directly in kernel context */
		pipeline_ops(ctx);
	}

	/* Step: Clean up - reset, disconnect, free buffer, free components, free pipeline */
	/* Reset pipeline to bring components back to COMP_STATE_READY,
	 * required before ipc_comp_free() which rejects non-READY components.
	 */
	ret = pipeline_reset(ctx->p, ctx->host_comp);
	zassert_equal(ret, 0, "pipeline reset failed");

	pipeline_disconnect(ctx->host_comp, ctx->buf, PPL_CONN_DIR_COMP_TO_BUFFER);
	pipeline_disconnect(ctx->dai_comp, ctx->buf, PPL_CONN_DIR_BUFFER_TO_COMP);

	buffer_free(ctx->buf);

	/* Free components through IPC to properly remove from IPC device list */
	ret = ipc_comp_free(ctx->ipc, ctx->host_comp->ipc_config.id);
	zassert_equal(ret, 0, "host comp free failed");

	ret = ipc_comp_free(ctx->ipc, ctx->dai_comp->ipc_config.id);
	zassert_equal(ret, 0, "DAI comp free failed");

	/* Unregister pipeline from IPC component list */
	list_item_del(&ctx->ipc_pipe->list);
	rfree(ctx->ipc_pipe);

	ret = pipeline_free(ctx->p);
	zassert_equal(ret, 0, "pipeline free failed");

	sof_heap_free(heap, ctx);
	LOG_INF("two component pipeline test complete");
}

ZTEST(userspace_ll, pipeline_two_components_kernel)
{
	pipeline_two_components(false);
}

ZTEST(userspace_ll, pipeline_two_components_user)
{
	pipeline_two_components(true);
}

ZTEST_SUITE(userspace_ll, NULL, NULL, NULL, NULL, NULL);

/**
 * SOF main has booted up and IPC handling is stopped.
 * Run test suites with ztest_run_all.
 */
static int run_tests(void)
{
	ztest_run_test_suite(userspace_ll, false, 1, 1, NULL);
	return 0;
}

SYS_INIT(run_tests, APPLICATION, 99);
