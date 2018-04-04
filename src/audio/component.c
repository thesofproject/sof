/*
 * Copyright (c) 2016, Intel Corporation
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
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/alloc.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <uapi/ipc.h>

struct comp_data {
	struct list_item list;		/* list of components */
	spinlock_t lock;
};

static struct comp_data *cd;

static struct comp_driver *get_drv(uint32_t type)
{
	struct list_item *clist;
	struct comp_driver *drv = NULL;

	spin_lock(&cd->lock);

	/* search driver list for driver type */
	list_for_item(clist, &cd->list) {

		drv = container_of(clist, struct comp_driver, list);
		if (drv->type == type)
			goto out;
	}

	/* not found */
	drv = NULL;

out:
	spin_unlock(&cd->lock);
	return drv;
}

struct comp_dev *comp_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *cdev;
	struct comp_driver *drv;

	/* find the driver for our new component */
	drv = get_drv(comp->type);
	if (drv == NULL) {
		trace_comp_error("eCD");
		trace_error_value(comp->type);
		return NULL;
	}

	/* create the new component */
	cdev = drv->ops.new(comp);
	if (cdev == NULL) {
		trace_comp_error("eCN");
		return NULL;
	}

	/* init component */
	memcpy(&cdev->comp, comp, sizeof(*comp));
	cdev->drv = drv;
	spinlock_init(&cdev->lock);
	list_init(&cdev->bsource_list);
	list_init(&cdev->bsink_list);

	return cdev;
}

int comp_register(struct comp_driver *drv)
{
	spin_lock(&cd->lock);
	list_item_prepend(&drv->list, &cd->list);
	spin_unlock(&cd->lock);

	return 0;
}

void comp_unregister(struct comp_driver *drv)
{
	spin_lock(&cd->lock);
	list_item_del(&drv->list);
	spin_unlock(&cd->lock);
}

int comp_set_state(struct comp_dev *dev, int cmd)
{
	int ret = 0;

	switch (cmd) {
	case COMP_TRIGGER_START:
		if (dev->state == COMP_STATE_PREPARE ||
			dev->state == COMP_STATE_PAUSED) {
			dev->state = COMP_STATE_ACTIVE;
		} else {
			trace_comp_error("CES");
			trace_error_value(dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_RELEASE:
		if (dev->state == COMP_STATE_PAUSED) {
			dev->state = COMP_STATE_ACTIVE;
		} else {
			trace_comp_error("CEr");
			trace_error_value(dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_STOP:
	case COMP_TRIGGER_XRUN:
		if (dev->state == COMP_STATE_ACTIVE) {
			dev->state = COMP_STATE_PREPARE;
		} else {
			trace_comp_error("CEs");
			trace_error_value(dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_PAUSE:
		/* only support pausing for running */
		if (dev->state == COMP_STATE_ACTIVE)
			dev->state = COMP_STATE_PAUSED;
		else {
			trace_comp_error("CEp");
			trace_error_value(dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_RESET:
		/* reset always succeeds */
		dev->state = COMP_STATE_READY;
		if (dev->state == COMP_STATE_ACTIVE ||
			dev->state == COMP_STATE_PAUSED) {
			trace_comp_error("CER");
			trace_error_value(dev->state);
			ret = 0;
		}
		break;
	case COMP_TRIGGER_PREPARE:
		if (dev->state == COMP_STATE_PREPARE ||
			dev->state == COMP_STATE_READY) {
			dev->state = COMP_STATE_PREPARE;
		} else {
			trace_comp_error("CEP");
			trace_error_value(dev->state);
			ret = -EINVAL;
		}
		break;
	default:
		break;
	}

	return ret;
}

void sys_comp_init(void)
{
	cd = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*cd));
	list_init(&cd->list);
	spinlock_init(&cd->lock);
}
