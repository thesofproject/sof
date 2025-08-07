// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019-2023 Intel Corporation. All rights reserved.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/ipc-config.h>
#include <rtos/init.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <sof/common.h>
#include <rtos/string.h>
#include <ipc/stream.h>
#include <user/trace.h>
#include <errno.h>
#include "asrc.h"

SOF_DEFINE_REG_UUID(asrc);

DECLARE_TR_CTX(asrc_tr, SOF_UUID(asrc_uuid), LOG_LEVEL_INFO);

int asrc_dai_configure_timestamp(struct comp_data *cd)
{
	if (cd->dai_dev)
		return cd->dai_dev->drv->ops.dai_ts_config(cd->dai_dev);

	return -EINVAL;
}

int asrc_dai_start_timestamp(struct comp_data *cd)
{
	if (cd->dai_dev)
		return cd->dai_dev->drv->ops.dai_ts_start(cd->dai_dev);

	return -EINVAL;
}

int asrc_dai_stop_timestamp(struct comp_data *cd)
{
	if (cd->dai_dev)
		return cd->dai_dev->drv->ops.dai_ts_stop(cd->dai_dev);

	return -EINVAL;
}

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
int asrc_dai_get_timestamp(struct comp_data *cd, struct dai_ts_data *tsd)
#else
int asrc_dai_get_timestamp(struct comp_data *cd, struct timestamp_data *tsd)
#endif
{
	if (!cd->dai_dev)
		return -EINVAL;

	return cd->dai_dev->drv->ops.dai_ts_get(cd->dai_dev, tsd);
}

void asrc_update_buffer_format(struct comp_buffer *buf_c, struct comp_data *cd)
{
	/* IPC3 don't need to update audio stream format here. */
}

void asrc_set_stream_params(struct comp_data *cd, struct sof_ipc_stream_params *params)
{
	/* IPC3 don't need to update audio stream format here. */
}
