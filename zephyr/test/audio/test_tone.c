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

extern void sys_comp_module_tone_interface_init(void);

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

	sys_comp_module_tone_interface_init();
	return NULL;
}

/* TONE_UUID extracted from native registries */
SOF_DEFINE_UUID("tone_test", tone_test_uuid, 0x04e3f894, 0x2c5c, 0x4f2e,
		0x8d, 0xc1, 0x69, 0x4e, 0xea, 0xab, 0x53, 0xfa);

struct custom_ipc4_config_tone {
	struct ipc4_base_module_cfg base;
} __attribute__((packed, aligned(4)));

/* Test TONE initialization */
ZTEST(audio_tone, test_tone_init)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct custom_ipc4_config_tone tone_init_data;

	memset(&tone_init_data, 0, sizeof(tone_init_data));
	memset(&tone_init_data.base.audio_fmt, 0, sizeof(tone_init_data.base.audio_fmt));
	tone_init_data.base.audio_fmt.channels_count = 2;
	tone_init_data.base.audio_fmt.sampling_frequency = 48000;
	tone_init_data.base.audio_fmt.depth = 32;
	tone_init_data.base.audio_fmt.valid_bit_depth = 32;
	tone_init_data.base.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;

	/* Setup basic IPC configuration */
	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 1;
	ipc_config.pipeline_id = 1;
	ipc_config.core = 0;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(tone_init_data);
	spec.data = (unsigned char *)&tone_init_data;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	/* Find TONE driver by UUID */
	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &tone_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for TONE not found");

	/* Initialize TONE component via ops */
	comp = drv->ops.create(drv, &ipc_config, &spec);
	zassert_not_null(comp, "TONE allocation failed");

	/* Verify component state */
	zassert_equal(comp->state, COMP_STATE_READY, "Component is not in READY state");
	zassert_equal(comp->ipc_config.id, 1, "IPC ID mismatch");

	/* Free the component */
	drv->ops.free(comp);
}

/* Test TONE parameters parsing */
ZTEST(audio_tone, test_tone_params)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct sof_ipc_stream_params params;
	struct custom_ipc4_config_tone tone_init_data;
	int ret;

	memset(&tone_init_data, 0, sizeof(tone_init_data));
	memset(&tone_init_data.base.audio_fmt, 0, sizeof(tone_init_data.base.audio_fmt));
	tone_init_data.base.audio_fmt.channels_count = 2;
	tone_init_data.base.audio_fmt.sampling_frequency = 48000;
	tone_init_data.base.audio_fmt.depth = 32;
	tone_init_data.base.audio_fmt.valid_bit_depth = 32;
	tone_init_data.base.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;

	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 1;
	ipc_config.pipeline_id = 1;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(tone_init_data);
	spec.data = (unsigned char *)&tone_init_data;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &tone_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for TONE not found");

	comp = drv->ops.create(drv, &ipc_config, &spec);

	memset(&params, 0, sizeof(params));
	params.channels = 2;
	params.rate = 48000;
	params.sample_container_bytes = 4;
	params.sample_valid_bytes = 4;
	params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;

	/* Configure parameters */
	ret = drv->ops.params(comp, &params);
	zassert_true(ret >= 0, "TONE parameter setup failed");

	drv->ops.free(comp);
}

ZTEST_SUITE(audio_tone, NULL, setup, NULL, NULL, NULL);
