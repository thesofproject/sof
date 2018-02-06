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
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>

struct comp_data {
	struct list_item list;		/* list of components */
	spinlock_t lock;
};

static struct comp_data *cd;

static void comp_init(struct comp_dev *dev, struct pipeline *p, uint32_t id,
	struct comp_driver *drv, uint32_t direction)
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
	uint32_t id, uint32_t direction)
{
	struct list_item *clist;
	struct comp_driver *drv;
	struct comp_dev *dev = NULL;

	spin_lock(&cd->lock);

	list_for_item(clist, &cd->list) {

		drv = container_of(clist, struct comp_driver, list);
		if (drv->type == type) {
			dev = drv->ops.new(type, index, direction);
			if (dev != NULL)
				comp_init(dev, p, id, drv, direction);
			else
				trace_comp_error("eCN");

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

void sys_comp_init(void)
{
	cd = rzalloc(RZONE_DEV, RMOD_SYS, sizeof(*cd));
	list_init(&cd->list);
	spinlock_init(&cd->lock);
}
