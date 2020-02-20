// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
//

 /**
  * \file drivers/generic/echo-ref.c
  * \brief Driver for Echo Reference Signal Virtual DAI
  * \author Artur Kloniecki <arturx.kloniecki@linux.intel.com>
  */

#include <ipc/dai.h>
#include <sof/drivers/echo-ref.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

static int echo_ref_probe(struct dai *dai)
{
	trace_echo("echo_ref_probe()");
	return 0;
}

static int echo_ref_remove(struct dai *dai)
{
	trace_echo("echo_ref_remove()");
	return 0;
}

static int echo_ref_set_config(struct dai *dai,
			       struct sof_ipc_dai_config *config)
{
	trace_echo("echo_ref_set_config()");
	return 0;
}

static int echo_ref_trigger(struct dai *dai, int cmd, int direction)
{
	trace_echo("echo_ref_trigger()");
	return 0;
}

static int echo_ref_get_handshake(struct dai *dai, int direction, int stream_id)
{
	trace_echo("echo_ref_get_handshake()");
	return 0;
}

static int echo_ref_get_fifo(struct dai *dai, int direction, int stream_id)
{
	trace_echo("echo_ref_get_fifo()");
	return 0;
}

static int echo_ref_dummy(struct dai *dai)
{
	trace_echo("echo_ref_dummy()");
	return 0;
}

static int echo_ref_ts_config(struct dai *dai, struct timestamp_cfg *cfg)
{
	trace_echo("echo_ref_ts_config()");
	return 0;
}

static int echo_ref_ts_start(struct dai *dai, struct timestamp_cfg *cfg)
{
	trace_echo("echo_ref_ts_start()");
	return 0;
}

static int echo_ref_ts_stop(struct dai *dai, struct timestamp_cfg *cfg)
{
	trace_echo("echo_ref_ts_stop()");
	return 0;
}

static int echo_ref_ts_get(struct dai *dai, struct timestamp_cfg *cfg,
			   struct timestamp_data *tsd)
{
	trace_echo("echo_ref_ts_get()");
	return 0;
}

const struct dai_driver echo_ref_driver = {
	.type = SOF_DAI_ECHO_REF,
	.dma_caps = DMA_CAP_GP_LP | DMA_CAP_GP_HP,
	.dma_dev = DMA_DEV_BUFFER,
	.ops = {
		.set_config = echo_ref_set_config,
		.trigger = echo_ref_trigger,
		.pm_context_restore = echo_ref_dummy,
		.pm_context_store = echo_ref_dummy,
		.get_handshake = echo_ref_get_handshake,
		.get_fifo = echo_ref_get_fifo,
		.probe = echo_ref_probe,
		.remove = echo_ref_remove,
	},
	.ts_ops = {
		.ts_config = echo_ref_ts_config,
		.ts_start = echo_ref_ts_start,
		.ts_stop = echo_ref_ts_stop,
		.ts_get = echo_ref_ts_get,
	},
};
