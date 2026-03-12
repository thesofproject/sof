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

extern void sys_comp_module_aria_interface_init(void);

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

	sys_comp_module_aria_interface_init();
	return NULL;
}

SOF_DEFINE_UUID("aria_test", aria_test_uuid,
		0x99f7166d, 0x372c, 0x43ef,
		0x81, 0xf6, 0x22, 0x00, 0x7a, 0xa1, 0x5f, 0x03);

struct custom_ipc4_config_aria {
	struct ipc4_base_module_cfg base;
	uint32_t attenuation;
} __attribute__((packed, aligned(8)));

/* Test ARIA initialization */
ZTEST(audio_aria, test_aria_init)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct custom_ipc4_config_aria aria_init_data;

	memset(&aria_init_data, 0, sizeof(aria_init_data));
	memset(&aria_init_data.base.audio_fmt, 0, sizeof(aria_init_data.base.audio_fmt));
	aria_init_data.base.ibs = 4096;
	aria_init_data.base.obs = 4096;
	aria_init_data.base.audio_fmt.channels_count = 2;
	aria_init_data.base.audio_fmt.sampling_frequency = 48000;
	aria_init_data.base.audio_fmt.depth = 32;
	aria_init_data.base.audio_fmt.valid_bit_depth = 32;
	aria_init_data.base.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;
	aria_init_data.attenuation = 1;

	/* Setup basic IPC configuration */
	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 1;
	ipc_config.pipeline_id = 1;
	ipc_config.core = 0;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(aria_init_data);
	spec.data = (unsigned char *)&aria_init_data;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	/* Find driver by UUID */
	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &aria_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for ARIA not found");

	/* Initialize component via ops */
	comp = drv->ops.create(drv, &ipc_config, &spec);
	zassert_not_null(comp, "ARIA allocation failed");

	/* Verify component state */
	zassert_equal(comp->state, COMP_STATE_READY, "Component is not in READY state");
	zassert_equal(comp->ipc_config.id, 1, "IPC ID mismatch");

	/* Free the component */
	drv->ops.free(comp);
}

ZTEST_SUITE(audio_aria, NULL, setup, NULL, NULL, NULL);
