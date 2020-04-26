// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Marcin Maka <marcin.maka@linux.intel.com>

#include <sof/lib/dai.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* 06711c94-d37d-4a76-b302-bbf6944fdd2b */
DECLARE_SOF_UUID("dai", dai_uuid, 0x06711c94, 0xd37d, 0x4a76,
		 0xb3, 0x02, 0xbb, 0xf6, 0x94, 0x4f, 0xdd, 0x2b);

DECLARE_TR_CTX(dai_tr, SOF_UUID(dai_uuid), LOG_LEVEL_INFO);

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

		tr_info(&dai_tr, "dai_get %d.%d new sref %d",
			type, index, d->sref);

		platform_shared_commit(d, sizeof(*d));

		spin_unlock(&d->lock);

		return !ret ? d : NULL;
	}
	tr_err(&dai_tr, "dai_get: %d.%d not found", type, index);
	return NULL;
}

void dai_put(struct dai *dai)
{
	int ret;

	spin_lock(&dai->lock);
	if (--dai->sref == 0) {
		ret = dai_remove(dai);
		if (ret < 0) {
			tr_err(&dai_tr, "dai_put: %d.%d dai_remove() failed ret = %d",
			       dai->drv->type, dai->index, ret);
		}
	}
	tr_info(&dai_tr, "dai_put %d.%d new sref %d",
		dai->drv->type, dai->index, dai->sref);
	platform_shared_commit(dai, sizeof(*dai));
	spin_unlock(&dai->lock);
}
