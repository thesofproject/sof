// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 NXP. All rights reserved.
//
// Author: Paul Olaru <paul.olaru@nxp.com>

#include <sof/audio/component.h>
#include <sof/audio/module.h>

#define MODULE_TYPE_TEMPLATE -1

static void template_free(struct comp_dev *dev)
{
	(void)dev;
}

static int template_params(struct comp_dev *dev)
{
	(void)dev;
	return 0;
}

static int template_cmd(struct comp_dev *dev, int cmd, void *data,
			int max_data_size)
{
	(void)dev;
	(void)cmd;
	(void)data;
	(void)max_data_size;
	return 0;
}

static int template_copy(struct comp_dev *dev)
{
	/* This one should actually copy something */
	(void)dev;
	return 0;
}

static int template_reset(struct comp_dev *dev)
{
	(void)dev;
	return 0;
}

static int template_prepare(struct comp_dev *dev)
{
	/* Just accept anything */
	(void)dev;
	return 0;
}

static int template_trigger(struct comp_dev *dev, int cmd)
{
	return comp_set_state(dev, cmd);
}

struct registered_module mod = {
	.module_type = MODULE_TYPE_TEMPLATE,
	.ops = {
		.free		= template_free,
		.params		= template_params,
		.cmd		= template_cmd,
		.copy		= template_copy,
		.prepare	= template_prepare,
		.reset		= template_reset,
		.trigger	= template_trigger,
	},
};

static void comp_module_template_init(void)
{
	register_module(&mod);
}

DECLARE_MODULE(comp_module_template_init);
