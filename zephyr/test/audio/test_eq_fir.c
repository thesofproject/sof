// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <string.h>
#include <rtos/sof.h>
#include <sof/list.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/topology.h>
#include <ipc4/base-config.h>

#include <rtos/alloc.h>
#include <user/eq.h>
#include <user/fir.h>
#include "test_audio_helper.h"

extern void sys_comp_module_eq_fir_interface_init(void);

static void *setup(void)
{
	struct sof *sof = sof_get();

	sys_comp_init(sof);

	if (!sof->ipc) {
		sof->ipc = rzalloc(SOF_MEM_FLAG_COHERENT, sizeof(*sof->ipc));
		sof->ipc->comp_data = rzalloc(SOF_MEM_FLAG_COHERENT, 4096);
		k_spinlock_init(&sof->ipc->lock);
		list_init(&sof->ipc->msg_list);
		list_init(&sof->ipc->comp_list);
	}

	sys_comp_module_eq_fir_interface_init();
	return NULL;
}

/* EQ_FIR_UUID */
SOF_DEFINE_UUID("eq_fir_test", eq_fir_test_uuid, 0x43a90ce7, 0xf3a5, 0x41df,
		0xac, 0x06, 0xba, 0x98, 0x65, 0x1a, 0xe6, 0xa3);

struct custom_ipc4_config_eq_fir {
	struct ipc4_base_module_cfg base;
} __attribute__((packed, aligned(4)));

/* Test EQ_FIR initialization */
static struct comp_dev *test_eq_fir_create(void)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct custom_ipc4_config_eq_fir init_data;

	memset(&init_data, 0, sizeof(init_data));
	memset(&init_data.base.audio_fmt, 0, sizeof(init_data.base.audio_fmt));
	init_data.base.audio_fmt.channels_count = 2;
	init_data.base.audio_fmt.sampling_frequency = 48000;
	init_data.base.audio_fmt.depth = 32;
	init_data.base.audio_fmt.valid_bit_depth = 32;
	init_data.base.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;

	/* Setup basic IPC configuration */
	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 1;
	ipc_config.pipeline_id = 1;
	ipc_config.core = 0;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(init_data);
	spec.data = (unsigned char *)&init_data;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	/* Find EQ_FIR driver by UUID */
	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &eq_fir_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for EQ_FIR not found");

	/* Initialize EQ_FIR component via ops */
	comp = drv->ops.create(drv, &ipc_config, &spec);
	zassert_not_null(comp, "Component allocation failed");

	return comp;
}

ZTEST(audio_eq_fir, test_eq_fir_init)
{
	struct comp_dev *comp = test_eq_fir_create();

	/* Verify component state */
	zassert_equal(comp->state, COMP_STATE_READY, "Component is not in READY state");
	zassert_equal(comp->ipc_config.id, 1, "IPC ID mismatch");

	/* Free the component */
	comp->drv->ops.free(comp);
}

/* Use generated passthrough config blob */
#include "blobs/eq_fir_passthrough_2ch.h"

ZTEST(audio_eq_fir, test_eq_fir_config)
{
	struct comp_dev *comp = test_eq_fir_create();

	/* Apply generated config blob before processing (skip 32-byte ABI header) */
	if (comp->drv->ops.set_large_config) {
		int ret = comp->drv->ops.set_large_config(comp, 1, true, true,
			sizeof(eq_fir_blob) - 32, (uint8_t *)eq_fir_blob + 32);
		zassert_equal(ret, 0, "set_large_config failed");
	}

	comp->drv->ops.free(comp);
}

ZTEST(audio_eq_fir, test_eq_fir_process)
{
	struct comp_dev *comp = test_eq_fir_create();

	/* Apply generated config blob before processing (skip 32-byte ABI header) */
	if (comp->drv->ops.set_large_config) {
		int ret = comp->drv->ops.set_large_config(comp, 1, true, true,
			sizeof(eq_fir_blob) - 32, (uint8_t *)eq_fir_blob + 32);
		zassert_true(ret >= 0, "set_large_config failed: %d", ret);
	}

	struct sof_ipc_stream_params params = {0};
	params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params.channels = 2;
	params.rate = 48000;
	params.sample_container_bytes = 4;
	params.sample_valid_bytes = 4;

	test_audio_helper_setup_buffers(comp, 4096, &params);
	test_audio_helper_process(comp);
	test_audio_helper_free_buffers(comp);

	comp->drv->ops.free(comp);
}

ZTEST(audio_eq_fir, test_eq_fir_params)
{
	struct comp_dev *comp = test_eq_fir_create();
	struct sof_ipc_stream_params params;
	int ret;

	memset(&params, 0, sizeof(params));
	params.channels = 2;
	params.rate = 48000;
	params.sample_container_bytes = 4;
	params.sample_valid_bytes = 4;
	params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;

	/* Configure parameters */
	ret = comp->drv->ops.params(comp, &params);
	zassert_true(ret >= 0, "EQ_FIR parameter setup failed");

	comp->drv->ops.free(comp);
}

ZTEST_SUITE(audio_eq_fir, NULL, setup, NULL, NULL, NULL);
