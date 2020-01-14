// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Marcin Maka <marcin.maka@linux.intel.com>

#include <sof/lib/dai.h>
#include <sof/lib/memory.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define trace_dai(__e, ...) trace_event(TRACE_CLASS_DAI, __e, ##__VA_ARGS__)

static inline const struct dai_type_info *dai_find_type(uint32_t type)
{
	const struct dai_info *info = dai_info_get();
	const struct dai_type_info *dti;

	for (dti = info->dai_type_array;
	     dti < info->dai_type_array + info->num_dai_types; dti++) {
		if (dti->type == type)
			return dti;
	}
	return NULL;
}

struct dai *dai_get(uint32_t type, uint32_t index, uint32_t flags)
{
	int ret = 0;
	const struct dai_type_info *dti;
	struct dai *d;

	dti = dai_find_type(type);
	if (!dti)
		return NULL; /* type not found */

	for (d = dti->dai_array; d < dti->dai_array + dti->num_dais; d++) {
		if (d->index != index) {
			platform_shared_commit(d, sizeof(*d));
			continue;
		}
		/* device created? */
		spin_lock(&d->lock);
		if (d->sref == 0) {
			if (flags & DAI_CREAT)
				ret = dai_probe(d);
			else
				ret = -ENODEV;
		}
		if (!ret)
			d->sref++;

		trace_dai("dai_get %d.%d new sref %d",
			  type, index, d->sref);

		platform_shared_commit(d, sizeof(*d));

		spin_unlock(&d->lock);

		return !ret ? d : NULL;
	}
	trace_error(TRACE_CLASS_DAI, "dai_get error: %d.%d not found",
		    type, index);
	return NULL;
}

void dai_put(struct dai *dai)
{
	int ret;

	spin_lock(&dai->lock);
	if (--dai->sref == 0) {
		ret = dai_remove(dai);
		if (ret < 0) {
			trace_error(TRACE_CLASS_DAI, "dai_put error: %d.%d dai_remove() failed ret = %d",
				    dai->drv->type, dai->index, ret);
		}
	}
	trace_event(TRACE_CLASS_DAI, "dai_put %d.%d new sref %d",
		    dai->drv->type, dai->index, dai->sref);
	platform_shared_commit(dai, sizeof(*dai));
	spin_unlock(&dai->lock);
}
