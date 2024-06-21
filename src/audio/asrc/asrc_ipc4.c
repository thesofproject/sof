// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019-2023 Intel Corporation. All rights reserved.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/ipc-config.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <sof/common.h>
#include <rtos/string.h>
#include <ipc/stream.h>
#include <user/trace.h>
#include <errno.h>
#include "asrc.h"

/* 66b4402d-b468-42f2-81a7-b37121863dd4 */
SOF_DEFINE_UUID("asrc", asrc_uuid, 0x66b4402d, 0xb468, 0x42f2,
		    0x81, 0xa7, 0xb3, 0x71, 0x21, 0x86, 0x3d, 0xd4);

DECLARE_TR_CTX(asrc_tr, SOF_UUID(asrc_uuid), LOG_LEVEL_INFO);

int asrc_dai_configure_timestamp(struct comp_data *cd)
{
	if (!cd->dai_dev)
		return -ENODEV;

	struct processing_module *mod = comp_mod(cd->dai_dev);
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;

	return ops->endpoint_ops->dai_ts_config(cd->dai_dev);
}

int asrc_dai_start_timestamp(struct comp_data *cd)
{
	if (!cd->dai_dev)
		return -ENODEV;

	struct processing_module *mod = comp_mod(cd->dai_dev);
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;

	return ops->endpoint_ops->dai_ts_start(cd->dai_dev);
}

int asrc_dai_stop_timestamp(struct comp_data *cd)
{
	if (!cd->dai_dev)
		return -ENODEV;

	struct processing_module *mod = comp_mod(cd->dai_dev);
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;

	return ops->endpoint_ops->dai_ts_stop(cd->dai_dev);
}

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
int asrc_dai_get_timestamp(struct comp_data *cd, struct dai_ts_data *tsd)
#else
int asrc_dai_get_timestamp(struct comp_data *cd, struct timestamp_data *tsd)
#endif
{
	if (!cd->dai_dev)
		return -ENODEV;

	struct processing_module *mod = comp_mod(cd->dai_dev);
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;

	return ops->endpoint_ops->dai_ts_get(cd->dai_dev, tsd);
}

void asrc_update_buffer_format(struct comp_buffer *buf_c, struct comp_data *cd)
{
	ipc4_update_buffer_format(buf_c, &cd->ipc_config.base.audio_fmt);
}

void asrc_set_stream_params(struct comp_data *cd, struct sof_ipc_stream_params *params)
{
	ipc4_base_module_cfg_to_stream_params(&cd->ipc_config.base, params);
}
