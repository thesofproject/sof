// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// These contents may have been developed with support from one or more
// Intel-operated generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
// Original helpers from test/cmocka/src/audio/pipeline/pipeline_connection_mocks.c:
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>

#include <zephyr/ztest.h>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/list.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>

#include <stdlib.h>

#include "pipeline_connection_mocks.h"

/* The scheduler list, normally created by scheduler_init() */
static struct schedulers *schedulers;

struct schedulers **arch_schedulers_get(void)
{
	return &schedulers;
}

/**
 * @brief Mock scheduler task free, marks the task as freed.
 * @param data Scheduler private data, unused.
 * @param task Task to free.
 * @return 0.
 */
static int schedule_task_mock_free(void *data, struct task *task)
{
	(void)data;

	task->state = SOF_TASK_STATE_FREE;
	task->ops.run = NULL;
	task->data = NULL;

	return 0;
}

static struct scheduler_ops schedule_mock_ops = {
	.schedule_task_free	= &schedule_task_mock_free,
};

static void *test_calloc(size_t size)
{
	void *ptr = calloc(1, size);

	zassert_not_null(ptr, "Failed to allocate %zu bytes", size);

	return ptr;
}

void cleanup_test_data(struct pipeline_connect_data *data)
{
	list_init(&data->first->bsource_list);
	list_init(&data->second->bsource_list);
	comp_buffer_reset_sink_list(data->b1);
	comp_buffer_reset_source_list(data->b1);
	list_init(&data->first->bsink_list);
	list_init(&data->second->bsink_list);
	comp_buffer_reset_sink_list(data->b2);
	comp_buffer_reset_source_list(data->b2);
}

struct pipeline_connect_data *get_standard_connect_objects(void)
{
	struct pipeline_connect_data *pipeline_connect_data =
		test_calloc(sizeof(struct pipeline_connect_data));
	struct pipeline *pipe = &pipeline_connect_data->p;
	struct schedule_data *sch;
	struct comp_dev *first;
	struct comp_dev *second;
	struct comp_buffer *buffer;
	struct comp_buffer *buffer_2;

	pipe->alloc = test_calloc(sizeof(*pipe->alloc));
	pipe->frames_per_sched = 5;
	pipe->pipeline_id = PIPELINE_ID_SAME;
	pipe->status = COMP_STATE_INIT;

	pipe->pipe_task = test_calloc(sizeof(struct task));
	pipe->pipe_task->type = SOF_SCHEDULE_EDF;

	schedulers = test_calloc(sizeof(struct schedulers));
	list_init(&schedulers->list);

	sch = test_calloc(sizeof(struct schedule_data));
	list_init(&sch->list);
	sch->type = SOF_SCHEDULE_EDF;
	sch->ops = &schedule_mock_ops;
	list_item_append(&sch->list, &schedulers->list);

	/* Bind the scheduler to the task, as schedule_task_init() would */
	pipe->pipe_task->sch = sch;

	first = test_calloc(sizeof(struct comp_dev));
	first->ipc_config.id = 3;
	first->ipc_config.pipeline_id = PIPELINE_ID_SAME;
	list_init(&first->bsink_list);
	list_init(&first->bsource_list);
	pipeline_connect_data->first = first;
	pipe->sched_comp = first;

	second = test_calloc(sizeof(struct comp_dev));
	second->ipc_config.id = 4;
	second->ipc_config.pipeline_id = PIPELINE_ID_DIFFERENT;
	list_init(&second->bsink_list);
	list_init(&second->bsource_list);
	pipeline_connect_data->second = second;

	buffer = test_calloc(sizeof(struct comp_buffer));
	comp_buffer_set_source_component(buffer, first);
	comp_buffer_set_sink_component(buffer, second);
	comp_buffer_reset_sink_list(buffer);
	comp_buffer_reset_source_list(buffer);
	pipeline_connect_data->b1 = buffer;

	buffer_2 = test_calloc(sizeof(struct comp_buffer));
	comp_buffer_set_source_component(buffer_2, second);
	comp_buffer_reset_sink_list(buffer_2);
	comp_buffer_reset_source_list(buffer_2);
	pipeline_connect_data->b2 = buffer_2;

	return pipeline_connect_data;
}

void free_standard_connect_objects(struct pipeline_connect_data *data)
{
	struct schedule_data *sch = data->p.pipe_task->sch;

	list_item_del(&sch->list);
	free(sch);
	free(schedulers);
	schedulers = NULL;

	free(data->p.alloc);
	free(data->p.pipe_task);
	free(data->first);
	free(data->second);
	free(data->b1);
	free(data->b2);
	free(data);
}
