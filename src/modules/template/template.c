// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Inc. All rights reserved.
//

#include <stdint.h>
#include <stddef.h>
#include <sof/module.h>
#include <sof/audio/component.h>

/* b77e677e-5ff4-4188-af14-fba8bdbf8682 */
DECLARE_SOF_RT_UUID("template", template_uuid, 0xb77e677e, 0x5ff4, 0x4188,
		    0xaf, 0x14, 0xfb, 0xa8, 0xbd, 0xbf, 0x86, 0x82);

DECLARE_TR_CTX(template_tr, SOF_UUID(template_uuid), LOG_LEVEL_INFO);

/* BSS test */
static int template_test;

static struct comp_dev *template_new(const struct comp_driver *drv,
		   struct comp_ipc_config *config,
		   void *spec)
{
	struct comp_dev *dev;

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;
	comp_info(dev, "template_new");
	template_test++;

	return dev;
}

static void template_free(struct comp_dev *dev)
{
	comp_info(dev, "template_free");
	rfree(dev);
}

/* set component audio stream parameters */
static int template_params(struct comp_dev *dev,
			   struct sof_ipc_stream_params *params)
{
	comp_info(dev, "template_params");
	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int template_cmd(struct comp_dev *dev, int cmd, void *data,
			int max_data_size)
{
	/* template will use buffer "connected" status */
	comp_info(dev, "template_cmd");
	return 0;
}

/* copy and process stream data from source to sink buffers */
static int template_copy(struct comp_dev *dev)
{
	comp_info(dev, "template_copy");
	return 0;
}

static int template_reset(struct comp_dev *dev)
{
	comp_info(dev, "template_reset");
	return 0;
}

static int template_prepare(struct comp_dev *dev)
{
	comp_info(dev, "template_prepare");
	return 0;
}

static const struct comp_driver comp_template = {
	.type	= SOF_COMP_NONE,
	.uid	= SOF_RT_UUID(template_uuid),
	.tctx	= &template_tr,
	.ops	= {
		.create		= template_new,
		.free		= template_free,
		.params		= template_params,
		.cmd		= template_cmd,
		.copy		= template_copy,
		.prepare	= template_prepare,
		.reset		= template_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_template_info = {
	.drv = &comp_template,
};

static int template_init(struct sof_module *mod)
{
	comp_register(&comp_template_info);
	return 0;
}

static int template_exit(struct sof_module *mod)
{
	comp_unregister(&comp_template_info);
	return 0;
}

// #if CONFIG_MODULE
SOF_MODULE(template, template_init, template_exit, 0xb77e677e, 0x5ff4, 0x4188,
		    0xaf, 0x14, 0xfb, 0xa8, 0xbd, 0xbf, 0x86, 0x82);
// #endif
