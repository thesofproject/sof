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
#include "../../../src/audio/crossover/crossover_user.h"

#include <rtos/alloc.h>

extern void sys_comp_module_crossover_interface_init(void);

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

	sys_comp_module_crossover_interface_init();
	return NULL;
}

SOF_DEFINE_UUID("crossover_test", crossover_test_uuid,
		0x948c9ad1, 0x806a, 0x4131,
		0xad, 0x6c, 0xb2, 0xbd, 0xa9, 0xe3, 0x5a, 0x9f);

struct custom_ipc4_config_crossover {
	struct ipc4_base_module_cfg base;
	struct ipc4_base_module_cfg_ext base_ext;
	struct ipc4_input_pin_format in_pins[1];
	struct ipc4_output_pin_format out_pins[2];
	struct sof_crossover_config config;
	struct sof_eq_iir_biquad coef[2];
} __attribute__((packed, aligned(8)));

/* Test Crossover initialization */
ZTEST(audio_crossover, test_crossover_init)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct custom_ipc4_config_crossover init_data;

	memset(&init_data, 0, sizeof(init_data));
	init_data.base.audio_fmt.channels_count = 2;
	init_data.base.audio_fmt.sampling_frequency = 48000;
	init_data.base.audio_fmt.depth = 16;
	init_data.base.audio_fmt.valid_bit_depth = 16;
	init_data.base.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;
	init_data.base.ibs = 2048;
	init_data.base.obs = 2048;

	/* Setup base extension and pins */
	init_data.base_ext.nb_input_pins = 1;
	init_data.base_ext.nb_output_pins = 2;
	init_data.in_pins[0].pin_index = 0;
	init_data.in_pins[0].ibs = 2048;
	init_data.out_pins[0].pin_index = 0;
	init_data.out_pins[0].obs = 2048;
	init_data.out_pins[1].pin_index = 1;
	init_data.out_pins[1].obs = 2048;

	/* Setup crossover specific parameters */
	init_data.config.size = sizeof(struct sof_crossover_config) + sizeof(struct sof_eq_iir_biquad) * 2;
	init_data.config.num_sinks = 2; // 2-way crossover
	init_data.config.assign_sink[0] = 0;
	init_data.config.assign_sink[1] = 1;

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
		if (!memcmp(info->drv->uid, &crossover_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for Crossover not found");

	/* Initialize component via ops */
	comp = drv->ops.create(drv, &ipc_config, &spec);
	zassert_not_null(comp, "Crossover allocation failed");

	/* Verify component state */
	zassert_equal(comp->state, COMP_STATE_READY, "Component is not in READY state");
	zassert_equal(comp->ipc_config.id, 1, "IPC ID mismatch");

	/* Free the component */
	drv->ops.free(comp);
}

ZTEST_SUITE(audio_crossover, NULL, setup, NULL, NULL, NULL);
