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
#include "../../../src/audio/copier/copier.h"

#include <rtos/alloc.h>

extern void sys_comp_module_copier_interface_init(void);

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

	sys_comp_module_copier_interface_init();
	return NULL;
}

/* Simple UUID for testing */
SOF_DEFINE_UUID("copier_test", copier_test_uuid, 0x9ba00c83, 0xca12, 0x4a83,
		0x94, 0x3c, 0x1f, 0xa2, 0xe8, 0x2f, 0x9d, 0xda);

/* Test copier initialization via module_adapter or directly */
ZTEST(audio_copier, test_copier_init)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct ipc4_copier_module_cfg copier_cfg;

	memset(&copier_cfg, 0, sizeof(copier_cfg));
	copier_cfg.base.ibs = 1024;
	copier_cfg.base.obs = 1024;
	copier_cfg.base.audio_fmt.sampling_frequency = 48000;
	copier_cfg.base.audio_fmt.depth = 16;
	copier_cfg.base.audio_fmt.valid_bit_depth = 16;
	copier_cfg.base.audio_fmt.channels_count = 2;
	copier_cfg.out_fmt = copier_cfg.base.audio_fmt;
	copier_cfg.gtw_cfg.node_id.dw = IPC4_INVALID_NODE_ID;

	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 1;
	ipc_config.pipeline_id = 1;
	ipc_config.core = 0;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(copier_cfg);
	spec.data = (const unsigned char *)&copier_cfg;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	/* Find copier driver by UUID */
	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &copier_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for copier not found");

	/* Initialize copier component via ops */
	comp = drv->ops.create(drv, &ipc_config, &spec);
	zassert_not_null(comp, "Copier allocation failed");

	/* Verify component state */
	zassert_equal(comp->state, COMP_STATE_READY, "Component is not in READY state");
	zassert_equal(comp->ipc_config.id, 1, "IPC ID mismatch");

	/* Free the component */
	drv->ops.free(comp);
}

/* Test copier parameters parsing */
ZTEST(audio_copier, test_copier_params)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct sof_ipc_stream_params params;
	int ret;

	struct ipc4_copier_module_cfg copier_cfg;
	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 1;
	ipc_config.pipeline_id = 1;

	memset(&copier_cfg, 0, sizeof(copier_cfg));
	copier_cfg.base.ibs = 1024;
	copier_cfg.base.obs = 1024;
	copier_cfg.base.audio_fmt.sampling_frequency = 48000;
	copier_cfg.base.audio_fmt.depth = 16;
	copier_cfg.base.audio_fmt.valid_bit_depth = 16;
	copier_cfg.base.audio_fmt.channels_count = 2;
	copier_cfg.out_fmt = copier_cfg.base.audio_fmt;
	copier_cfg.gtw_cfg.node_id.dw = IPC4_INVALID_NODE_ID;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(copier_cfg);
	spec.data = (const unsigned char *)&copier_cfg;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &copier_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for copier not found");

	comp = drv->ops.create(drv, &ipc_config, &spec);

	memset(&params, 0, sizeof(params));
	params.channels = 2;
	params.rate = 48000;
	params.sample_container_bytes = 2;
	params.sample_valid_bytes = 2;
	params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;

	/* Configure parameters */
	ret = drv->ops.params(comp, &params);
	/* Assuming parameters evaluate successfully relative to core formats */
	zassert_true(ret >= 0, "Copier parameter setup failed");

	drv->ops.free(comp);
}

ZTEST_SUITE(audio_copier, NULL, setup, NULL, NULL, NULL);
