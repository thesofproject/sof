/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/audio/component.h>

#include "comp_mock.h"

static struct comp_dev *mock_comp_new(struct sof_ipc_comp *comp)
{
	return calloc(1, sizeof(struct comp_dev));
}

static void mock_comp_free(struct comp_dev *dev)
{
	free(dev);
}

static int mock_comp_params(struct comp_dev *dev)
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

struct comp_driver comp_mock = {
	.type	= SOF_COMP_MOCK,
	.ops	= {
		.new		= mock_comp_new,
		.free		= mock_comp_free,
		.params		= mock_comp_params,
		.cmd		= mock_comp_cmd,
		.copy		= mock_comp_copy,
		.prepare	= mock_comp_prepare,
		.reset		= mock_comp_reset,
	},
};

void sys_comp_mock_init(void)
{
	comp_register(&comp_mock);
}
