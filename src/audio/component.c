// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/alloc.h>
#include <sof/list.h>
#include <sof/string.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

struct comp_data {
	struct list_item list;		/* list of components */
};

static struct comp_data *cd;

static struct comp_driver *get_drv(uint32_t type)
{
	struct list_item *clist;
	struct comp_driver *drv = NULL;
	uint32_t flags;

	irq_local_disable(flags);

	/* search driver list for driver type */
	list_for_item(clist, &cd->list) {

		drv = container_of(clist, struct comp_driver, list);
		if (drv->type == type)
			goto out;
	}

	/* not found */
	drv = NULL;

out:
	irq_local_enable(flags);
	return drv;
}

struct comp_dev *comp_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *cdev;
	struct comp_driver *drv;

	/* find the driver for our new component */
	drv = get_drv(comp->type);
	if (!drv) {
		trace_comp_error("comp_new() error: driver not found, "
				 "comp->type = %u", comp->type);
		return NULL;
	}

	/* create the new component */
	cdev = drv->ops.new(comp);
	if (!cdev) {
		trace_comp_error("comp_new() error: "
				 "unable to create the new component");
		return NULL;
	}

	/* init component */
	assert(!memcpy_s(&cdev->comp, sizeof(cdev->comp),
		comp, sizeof(*comp)));

	cdev->drv = drv;
	list_init(&cdev->bsource_list);
	list_init(&cdev->bsink_list);

	return cdev;
}

int comp_register(struct comp_driver *drv)
{
	uint32_t flags;

	irq_local_disable(flags);
	list_item_prepend(&drv->list, &cd->list);
	irq_local_enable(flags);

	return 0;
}

void comp_unregister(struct comp_driver *drv)
{
	uint32_t flags;

	irq_local_disable(flags);
	list_item_del(&drv->list);
	irq_local_enable(flags);
}

int comp_set_state(struct comp_dev *dev, int cmd)
{
	int requested_state = comp_get_requested_state(cmd);
	int ret = 0;

	if (dev->state == requested_state) {
		trace_comp("comp_set_state(), state already set to %u",
			   dev->state);
		return COMP_STATUS_STATE_ALREADY_SET;
	}

	switch (cmd) {
	case COMP_TRIGGER_START:
		if (dev->state == COMP_STATE_PREPARE) {
			dev->state = COMP_STATE_ACTIVE;
		} else {
			trace_comp_error("comp_set_state() error: "
					 "wrong state = %u, "
					 "COMP_TRIGGER_START", dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_RELEASE:
		if (dev->state == COMP_STATE_PAUSED) {
			dev->state = COMP_STATE_ACTIVE;
		} else {
			trace_comp_error("comp_set_state() error: "
					 "wrong state = %u, "
					 "COMP_TRIGGER_RELEASE", dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_STOP:
		if (dev->state == COMP_STATE_ACTIVE ||
		    dev->state == COMP_STATE_PAUSED) {
			dev->state = COMP_STATE_PREPARE;
		} else {
			trace_comp_error("comp_set_state() error: "
					 "wrong state = %u, "
					 "COMP_TRIGGER_STOP", dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_XRUN:
		/* reset component status to ready at xrun */
		dev->state = COMP_STATE_READY;
		break;
	case COMP_TRIGGER_PAUSE:
		/* only support pausing for running */
		if (dev->state == COMP_STATE_ACTIVE) {
			dev->state = COMP_STATE_PAUSED;
		} else {
			trace_comp_error("comp_set_state() error: "
					 "wrong state = %u, "
					 "COMP_TRIGGER_PAUSE", dev->state);
			ret = -EINVAL;
		}
		break;
	case COMP_TRIGGER_RESET:
		/* reset always succeeds */
		if (dev->state == COMP_STATE_ACTIVE ||
		    dev->state == COMP_STATE_PAUSED) {
			trace_comp_error("comp_set_state() error: "
					 "wrong state = %u, "
					 "COMP_TRIGGER_RESET", dev->state);
			ret = 0;
		}
		dev->state = COMP_STATE_READY;
		break;
	case COMP_TRIGGER_PREPARE:
		if (dev->state == COMP_STATE_READY) {
			dev->state = COMP_STATE_PREPARE;
		} else {
			trace_comp_error("comp_set_state() error: "
					 "wrong state = %u, "
					 "COMP_TRIGGER_PREPARE", dev->state);
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
}

int comp_get_copy_limits(struct comp_dev *dev, struct comp_copy_limits *cl)
{
	/* Get source and sink buffer addresses */
	cl->source = list_first_item(&dev->bsource_list, struct comp_buffer,
				     sink_list);
	cl->sink = list_first_item(&dev->bsink_list, struct comp_buffer,
				   source_list);

	cl->frames = comp_avail_frames(cl->source, cl->sink);
	cl->source_frame_bytes = comp_frame_bytes(cl->source->source);
	cl->sink_frame_bytes = comp_frame_bytes(cl->sink->sink);
	cl->source_bytes = cl->frames * cl->source_frame_bytes;
	cl->sink_bytes = cl->frames * cl->sink_frame_bytes;

	return 0;
}
