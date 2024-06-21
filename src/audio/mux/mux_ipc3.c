// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#if CONFIG_COMP_MUX

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <module/module/base.h>
#include <sof/trace/trace.h>
#include <rtos/string_macro.h>
#include <sof/lib/uuid.h>
#include <sof/common.h>
#include <sof/platform.h>
#include <ipc/topology.h>
#include <errno.h>

#include "mux.h"

LOG_MODULE_DECLARE(muxdemux, CONFIG_SOF_LOG_LEVEL);

/* c607ff4d-9cb6-49dc-b678-7da3c63ea557 */
SOF_DEFINE_UUID("mux", mux_uuid, 0xc607ff4d, 0x9cb6, 0x49dc,
		    0xb6, 0x78, 0x7d, 0xa3, 0xc6, 0x3e, 0xa5, 0x57);

DECLARE_TR_CTX(mux_tr, SOF_UUID(mux_uuid), LOG_LEVEL_INFO);

/* c4b26868-1430-470e-a089-15d1c77f851a */
SOF_DEFINE_UUID("demux", demux_uuid, 0xc4b26868, 0x1430, 0x470e,
		    0xa0, 0x89, 0x15, 0xd1, 0xc7, 0x7f, 0x85, 0x1a);

DECLARE_TR_CTX(demux_tr, SOF_UUID(demux_uuid), LOG_LEVEL_INFO);

static int mux_set_values(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_mux_config *cfg = &cd->config;
	unsigned int i;
	unsigned int j;

	comp_dbg(dev, "mux_set_values()");

	/* check if number of streams configured doesn't exceed maximum */
	if (cfg->num_streams > MUX_MAX_STREAMS) {
		comp_err(dev, "mux_set_values(): configured number of streams (%u) exceeds maximum = "
			    STRINGIFY(MUX_MAX_STREAMS), cfg->num_streams);
		return -EINVAL;
	}

	/* check if all streams configured have distinct IDs */
	for (i = 0; i < cfg->num_streams; i++) {
		for (j = i + 1; j < cfg->num_streams; j++) {
			if (cfg->streams[i].pipeline_id ==
				cfg->streams[j].pipeline_id) {
				comp_err(dev, "mux_set_values(): multiple configured streams have same pipeline ID = %u",
					 cfg->streams[i].pipeline_id);
				return -EINVAL;
			}
		}
	}

	for (i = 0; i < cfg->num_streams; i++) {
		for (j = 0 ; j < PLATFORM_MAX_CHANNELS; j++) {
			if (popcount(cfg->streams[i].mask[j]) > 1) {
				comp_err(dev, "mux_set_values(): mux component is not able to mix channels");
				return -EINVAL;
			}
		}
	}

	if (dev->ipc_config.type == SOF_COMP_MUX) {
		if (mux_mix_check(cfg))
			comp_err(dev, "mux_set_values(): mux component is not able to mix channels");
	}

	for (i = 0; i < cfg->num_streams; i++) {
		cd->config.streams[i].pipeline_id = cfg->streams[i].pipeline_id;
		for (j = 0; j < PLATFORM_MAX_CHANNELS; j++)
			cd->config.streams[i].mask[j] = cfg->streams[i].mask[j];
	}

	cd->config.num_streams = cfg->num_streams;

	if (dev->ipc_config.type == SOF_COMP_MUX)
		mux_prepare_look_up_table(mod);
	else
		demux_prepare_look_up_table(mod);

	if (dev->state > COMP_STATE_INIT) {
		if (dev->ipc_config.type == SOF_COMP_MUX)
			cd->mux = mux_get_processing_function(mod);
		else
			cd->demux = demux_get_processing_function(mod);
	}

	return 0;
}

int mux_params(struct processing_module *mod)
{
	return mux_set_values(mod);
}
#endif /* CONFIG_COMP_MUX */
