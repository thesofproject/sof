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

extern void sys_comp_module_asrc_interface_init(void);

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

	sys_comp_module_asrc_interface_init();
	return NULL;
}

/* Simple UUID for testing */
SOF_DEFINE_UUID("asrc_test", asrc_test_uuid, 0x66b4402d, 0xb468, 0x42f2,
		0x81, 0xa7, 0xb3, 0x71, 0x21, 0x86, 0x3d, 0xd4);

struct custom_ipc4_config_asrc {
	struct ipc4_base_module_cfg base;
	uint32_t out_freq;
	uint32_t asrc_mode;
} __attribute__((packed, aligned(4)));

/* Test ASRC initialization */
ZTEST(audio_asrc, test_asrc_init)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct custom_ipc4_config_asrc asrc_init_data;

	memset(&asrc_init_data, 0, sizeof(asrc_init_data));
	memset(&asrc_init_data.base.audio_fmt, 0, sizeof(asrc_init_data.base.audio_fmt));
	asrc_init_data.base.audio_fmt.channels_count = 2;
	asrc_init_data.base.audio_fmt.sampling_frequency = 48000;
	asrc_init_data.base.audio_fmt.depth = 32;
	asrc_init_data.base.audio_fmt.valid_bit_depth = 32;
	asrc_init_data.base.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;
	asrc_init_data.out_freq = 44100;
	asrc_init_data.asrc_mode = 0; /* PUSH MODE */

	/* Setup basic IPC configuration */
	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 1;
	ipc_config.pipeline_id = 1;
	ipc_config.core = 0;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(asrc_init_data);
	spec.data = (unsigned char *)&asrc_init_data;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	/* Find ASRC driver by UUID */
	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &asrc_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for ASRC not found");

	/* Initialize ASRC component via ops */
	comp = drv->ops.create(drv, &ipc_config, &spec);
	zassert_not_null(comp, "ASRC allocation failed");

	/* Verify component state */
	zassert_equal(comp->state, COMP_STATE_READY, "Component is not in READY state");
	zassert_equal(comp->ipc_config.id, 1, "IPC ID mismatch");

	/* Free the component */
	drv->ops.free(comp);
}

/* Test ASRC parameters parsing */
ZTEST(audio_asrc, test_asrc_params)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct sof_ipc_stream_params params;
	struct custom_ipc4_config_asrc asrc_init_data;
	int ret;

	memset(&asrc_init_data, 0, sizeof(asrc_init_data));
	memset(&asrc_init_data.base.audio_fmt, 0, sizeof(asrc_init_data.base.audio_fmt));
	asrc_init_data.base.audio_fmt.channels_count = 2;
	asrc_init_data.base.audio_fmt.sampling_frequency = 48000;
	asrc_init_data.base.audio_fmt.depth = 32;
	asrc_init_data.base.audio_fmt.valid_bit_depth = 32;
	asrc_init_data.base.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;
	asrc_init_data.out_freq = 44100;
	asrc_init_data.asrc_mode = 0;

	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 1;
	ipc_config.pipeline_id = 1;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(asrc_init_data);
	spec.data = (unsigned char *)&asrc_init_data;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &asrc_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for ASRC not found");

	comp = drv->ops.create(drv, &ipc_config, &spec);

	memset(&params, 0, sizeof(params));
	params.channels = 2;
	params.rate = 48000;
	params.sample_container_bytes = 4;
	params.sample_valid_bytes = 4;
	params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;

	/* Configure parameters */
	ret = drv->ops.params(comp, &params);
	zassert_true(ret >= 0, "ASRC parameter setup failed");

	drv->ops.free(comp);
}

ZTEST_SUITE(audio_asrc, NULL, setup, NULL, NULL, NULL);
