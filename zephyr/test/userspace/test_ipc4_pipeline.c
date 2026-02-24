// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

/*
 * Test case for creation and destruction of IPC4 pipelines.
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include <sof/ipc/topology.h>
#include <ipc4/pipeline.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/sof.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

ZTEST(userspace_ipc4_pipeline, test_pipeline_create_destroy_helpers)
{
	struct ipc *ipc = ipc_get();
	struct ipc4_pipeline_create msg = { 0 };
	struct ipc_comp_dev *ipc_pipe;
	int ret;

	LOG_INF("Starting IPC4 pipeline test (helpers)");

	/* 1. Setup msg */
	msg.primary.r.instance_id = 1;
	msg.primary.r.ppl_priority = SOF_IPC4_PIPELINE_PRIORITY_0;
	msg.primary.r.ppl_mem_size = 1;
	msg.primary.r.type = SOF_IPC4_GLB_CREATE_PIPELINE;
	msg.extension.r.core_id = 0;

	/* 2. Create pipeline */
	ret = ipc_pipeline_new(ipc, (ipc_pipe_new *)&msg);
	zassert_equal(ret, 0, "ipc_pipeline_new failed with %d", ret);

	/* 3. Validate pipeline exists */
	ipc_pipe = ipc_get_pipeline_by_id(ipc, 1);
	zassert_not_null(ipc_pipe, "pipeline 1 not found after creation");

	/* 4. Destroy pipeline */
	ret = ipc_pipeline_free(ipc, 1);
	zassert_equal(ret, 0, "ipc_pipeline_free failed with %d", ret);

	/* 5. Validate pipeline is destroyed */
	ipc_pipe = ipc_get_pipeline_by_id(ipc, 1);
	zassert_is_null(ipc_pipe, "pipeline 1 still exists after destruction");

	LOG_INF("IPC4 pipeline test (helpers) complete");
}

ZTEST(userspace_ipc4_pipeline, test_pipeline_create_destroy_handlers)
{
	struct ipc *ipc = ipc_get();
	struct ipc4_pipeline_create create_msg = { 0 };
	struct ipc4_message_request req = { 0 };
	struct ipc_comp_dev *ipc_pipe;
	int ret;

	LOG_INF("Starting IPC4 pipeline test (handlers)");

	/* 1. Setup create message */
	create_msg.primary.r.instance_id = 2;
	create_msg.primary.r.ppl_priority = SOF_IPC4_PIPELINE_PRIORITY_0;
	create_msg.primary.r.ppl_mem_size = 1;
	create_msg.primary.r.type = SOF_IPC4_GLB_CREATE_PIPELINE;
	create_msg.extension.r.core_id = 0;

	/* Pack the create message into a generic request */
	req.primary.dat = create_msg.primary.dat;
	req.extension.dat = create_msg.extension.dat;

	/* 2. Create pipeline using handler */
	ret = ipc4_new_pipeline(&req);
	zassert_equal(ret, 0, "ipc4_new_pipeline failed with %d", ret);

	/* 3. Validate pipeline exists */
	ipc_pipe = ipc_get_pipeline_by_id(ipc, 2);
	zassert_not_null(ipc_pipe, "pipeline 2 not found after creation");

	/* 4. Setup delete message */
	struct ipc4_pipeline_delete delete_msg = { 0 };

	delete_msg.primary.r.instance_id = 2;
	delete_msg.primary.r.type = SOF_IPC4_GLB_DELETE_PIPELINE;

	/* Pack the delete message into a generic request */
	req.primary.dat = delete_msg.primary.dat;
	req.extension.dat = delete_msg.extension.dat;

	/* Destroy pipeline using handler */
	ret = ipc4_delete_pipeline(&req);
	zassert_equal(ret, 0, "ipc4_delete_pipeline failed with %d", ret);

	/* 5. Validate pipeline is destroyed */
	ipc_pipe = ipc_get_pipeline_by_id(ipc, 2);
	zassert_is_null(ipc_pipe, "pipeline 2 still exists after destruction");

	LOG_INF("IPC4 pipeline test (handlers) complete");
}

static void *ipc4_pipeline_setup(void)
{
	struct sof *sof = sof_get();

	/* SOF_BOOT_TEST_STANDALONE skips IPC init. We must allocate it manually for testing. */
	if (!sof->ipc) {
		sof->ipc = rzalloc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT, sizeof(*sof->ipc));
		sof->ipc->comp_data = rzalloc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
					      SOF_IPC_MSG_MAX_SIZE);
		k_spinlock_init(&sof->ipc->lock);
		list_init(&sof->ipc->msg_list);
		list_init(&sof->ipc->comp_list);
	}

	return NULL;
}

ZTEST_SUITE(userspace_ipc4_pipeline, NULL, ipc4_pipeline_setup, NULL, NULL, NULL);


