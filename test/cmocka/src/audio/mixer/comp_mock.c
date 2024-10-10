// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <stdint.h>
#include <stddef.h>
#include <sof/list.h>
#include <sof/audio/stream.h>
#include <sof/audio/component.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#else
#include <stdlib.h>
#endif

#include "comp_mock.h"

static struct comp_dev *mock_comp_new(const struct comp_driver *drv,
				      const struct comp_ipc_config *config,
				      const void *spec)
{
	struct comp_dev *dev = comp_alloc(drv, sizeof(*dev));

	return dev;
}

static void mock_comp_free(struct comp_dev *dev)
{
	free(dev);
}

static int mock_comp_params(struct comp_dev *dev,
			    struct sof_ipc_stream_params *params)
{
	return 0;
}

static int mock_comp_cmd(struct comp_dev *dev, int cmd, void *data,
			 int max_data_size)
{
	return 0;
}

static int mock_comp_copy(struct comp_dev *dev)
{
	return 0;
}

static int mock_comp_reset(struct comp_dev *dev)
{
	return 0;
}

static int mock_comp_prepare(struct comp_dev *dev)
{
	return 0;
}

static const struct comp_driver comp_mock = {
	.type	= SOF_COMP_MOCK,
	.ops	= {
		.create		= mock_comp_new,
		.free		= mock_comp_free,
		.params		= mock_comp_params,
		.cmd		= mock_comp_cmd,
		.copy		= mock_comp_copy,
		.prepare	= mock_comp_prepare,
		.reset		= mock_comp_reset,
	},
};

static struct comp_driver_info comp_mock_info = {
	.drv = &comp_mock,
};

void sys_comp_mock_init(void)
{
	mock_comp_register(&comp_mock_info);
}

