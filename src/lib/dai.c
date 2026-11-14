// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Marcin Maka <marcin.maka@linux.intel.com>

#include <sof/lib/dai.h>
#include <rtos/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <rtos/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <ipc/dai.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(dai, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(dai_lib);

/* unused with Zephyr, generates no output */
DECLARE_TR_CTX(dai_tr, SOF_UUID(dai_lib_uuid), LOG_LEVEL_INFO);

/* The DAI device lookup and the DAI getters, which run in kernel mode, are in
 * src/kernel/lib/dai-kernel.c
 */

struct dai_group_list {
	struct list_item list;
} __aligned(PLATFORM_DCACHE_ALIGN);

static struct dai_group_list *groups[CONFIG_CORE_COUNT];

__cold static struct dai_group_list *dai_group_list_get(int core_id)
{
	struct dai_group_list *group_list = groups[core_id];

	assert_can_be_cold();

	if (!group_list) {
		group_list = rzalloc(SOF_MEM_FLAG_USER,
				     sizeof(*group_list));
		if (!group_list) {
			tr_err(&dai_tr, "dai_group_list_get(): failed to allocate group_list for core %d",
			       core_id);
			return NULL;
		}

		groups[core_id] = group_list;
		list_init(&group_list->list);
	}

	return group_list;
}

__cold static struct dai_group *dai_group_find(uint32_t group_id)
{
	struct list_item *dai_groups;
	struct dai_group_list *group_list;
	struct list_item *group_item;
	struct dai_group *group = NULL;

	assert_can_be_cold();

	group_list = dai_group_list_get(cpu_get_id());
	if (!group_list) {
		tr_err(&dai_tr, "dai_group_find(): failed to get group_list for core %d",
		       cpu_get_id());
		return NULL;
	}
	dai_groups = &group_list->list;

	list_for_item(group_item, dai_groups) {
		group = container_of(group_item, struct dai_group, list);

		if (group->group_id == group_id)
			break;

		group = NULL;
	}

	return group;
}

__cold static struct dai_group *dai_group_alloc(void)
{
	struct dai_group_list *group_list = dai_group_list_get(cpu_get_id());
	struct list_item *dai_groups;
	struct dai_group *group;

	assert_can_be_cold();
	if (!group_list) {
		tr_err(&dai_tr, "dai_group_alloc(): failed to get group_list for core %d",
		       cpu_get_id());
		return NULL;
	}
	dai_groups = &group_list->list;

	group = rzalloc(SOF_MEM_FLAG_USER,
			sizeof(*group));
	if (!group) {
		tr_err(&dai_tr, "dai_group_alloc(): failed to allocate dai group");
		return NULL;
	}

	list_item_prepend(&group->list, dai_groups);

	return group;
}

__cold struct dai_group *dai_group_get(uint32_t group_id, uint32_t flags)
{
	struct dai_group *group;

	assert_can_be_cold();

	if (!group_id) {
		tr_err(&dai_tr, "dai_group_get(): invalid group_id %u",
		       group_id);
		return NULL;
	}

	/* Check if this group already exists */
	group = dai_group_find(group_id);

	/* Check if there's a released and now unused group */
	if (!group)
		group = dai_group_find(0);

	/* Group doesn't exist, let's allocate and initialize it */
	if (!group && flags & DAI_CREAT)
		group = dai_group_alloc();

	if (group) {
		/* Group might've been previously unused */
		if (!group->group_id)
			group->group_id = group_id;

		group->num_dais++;
	} else {
		tr_err(&dai_tr, "dai_group_get(): failed to get group_id %u",
		       group_id);
	}

	return group;
}

__cold void dai_group_put(struct dai_group *group)
{
	assert_can_be_cold();

	group->num_dais--;

	/* Mark as unused if there are no more DAIs in this group */
	if (!group->num_dais)
		group->group_id = 0;
}
