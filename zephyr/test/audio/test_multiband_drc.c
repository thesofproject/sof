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
#include <sof/ipc/topology.h>
#include <ipc4/base-config.h>
#include "../../../src/audio/multiband_drc/user/multiband_drc.h"

#include <rtos/alloc.h>

extern void sys_comp_module_multiband_drc_interface_init(void);

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

	sys_comp_module_multiband_drc_interface_init();
	return NULL;
}

SOF_DEFINE_UUID("multiband_drc_test", multiband_drc_test_uuid,
		0x0d9f2256, 0x8e4f, 0x47b3,
		0x84, 0x48, 0x23, 0x9a, 0x33, 0x4f, 0x11, 0x91);

struct custom_ipc4_config_multiband_drc {
	struct ipc4_base_module_cfg base;
	struct sof_multiband_drc_config config;
	struct sof_drc_params drc_coef[2];
} __attribute__((packed, aligned(8)));

/* Test Multiband_DRC initialization */
ZTEST(audio_multiband_drc, test_multiband_drc_init)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct custom_ipc4_config_multiband_drc init_data;

	memset(&init_data, 0, sizeof(init_data));
	init_data.base.audio_fmt.channels_count = 2;
	init_data.base.audio_fmt.sampling_frequency = 48000;
	init_data.base.audio_fmt.depth = 16;
	init_data.base.audio_fmt.valid_bit_depth = 16;
	init_data.base.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;
	init_data.base.ibs = 2048;
	init_data.base.obs = 2048;

	/* Setup Multiband_DRC specific parameters */
	init_data.config.size = sizeof(struct sof_multiband_drc_config) + sizeof(struct sof_drc_params) * 2;
	init_data.config.num_bands = 2;
	init_data.config.enable_emp_deemp = 0;

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

	/* Find driver by UUID */
	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &multiband_drc_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for Multiband DRC not found");

	/* Initialize component via ops */
	comp = drv->ops.create(drv, &ipc_config, &spec);
	zassert_not_null(comp, "Multiband DRC allocation failed");

	/* Verify component state */
	zassert_equal(comp->state, COMP_STATE_READY, "Component is not in READY state");
	zassert_equal(comp->ipc_config.id, 1, "IPC ID mismatch");

	/* Free the component */
	drv->ops.free(comp);
}

ZTEST_SUITE(audio_multiband_drc, NULL, setup, NULL, NULL, NULL);
