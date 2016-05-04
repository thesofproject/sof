/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>

struct comp_data {
	struct list_head list;		/* list of components */
	spinlock_t lock;	
};

static struct comp_data *cd;

static void comp_init(struct comp_dev *dev, struct pipeline *p, uint32_t id,
	struct comp_driver *drv, uint8_t direction)
{
	dev->id = id;
	dev->drv = drv;
	dev->direction = direction;
	dev->state = COMP_STATE_INIT;
	dev->pipeline = p;
	spinlock_init(&dev->lock);
	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);
}

struct comp_dev *comp_new(struct pipeline *p, uint32_t type, uint32_t index,
	uint32_t id, uint8_t direction)
{
	struct list_head *clist;
	struct comp_driver *drv;
	struct comp_dev *dev = NULL;

	spin_lock(&cd->lock);

	list_for_each(clist, &cd->list) {

		drv = container_of(clist, struct comp_driver, list);
		if (drv->type == type) {
			dev = drv->ops.new(type, index, direction);
			comp_init(dev, p, id, drv, direction);
			goto out;
		}
	}

out:
	spin_unlock(&cd->lock);
	return dev;
}

int comp_register(struct comp_driver *drv)
{
	spin_lock(&cd->lock);
	list_add(&drv->list, &cd->list);
	spin_unlock(&cd->lock);

	return 0;
}

void comp_unregister(struct comp_driver *drv)
{
	spin_lock(&cd->lock);
	list_del(&drv->list);
	spin_unlock(&cd->lock);
}

void sys_comp_init(void)
{
	cd = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(*cd));
	list_init(&cd->list);
	spinlock_init(&cd->lock);
}
