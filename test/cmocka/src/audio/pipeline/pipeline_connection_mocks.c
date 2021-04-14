// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>

#include "pipeline_connection_mocks.h"

extern struct schedulers *schedulers;

struct scheduler_ops schedule_mock_ops = {
	.schedule_task_free	= &schedule_task_mock_free,
};

void cleanup_test_data(struct pipeline_connect_data *data)
{
	list_init(&data->first->bsource_list);
	list_init(&data->second->bsource_list);
	list_init(&data->b1->sink_list);
	list_init(&data->b1->source_list);
	list_init(&data->first->bsink_list);
	list_init(&data->second->bsink_list);
	list_init(&data->b2->sink_list);
	list_init(&data->b2->source_list);
}

struct pipeline_connect_data *get_standard_connect_objects(void)
{
	struct pipeline_connect_data *pipeline_connect_data = calloc
		(sizeof(struct pipeline_connect_data), 1);

	struct pipeline *pipe = calloc(sizeof(struct pipeline), 1);

	pipe->frames_per_sched = 5;
	pipe->pipeline_id = PIPELINE_ID_SAME;
	pipe->status = COMP_STATE_INIT;

	pipe->pipe_task = calloc(sizeof(struct task), 1);
	pipe->pipe_task->type = SOF_SCHEDULE_EDF;

	schedulers = calloc(sizeof(struct schedulers), 1);
	list_init(&schedulers->list);

	struct schedule_data *sch = calloc(sizeof(struct schedule_data), 1);

	list_init(&sch->list);
	sch->type = SOF_SCHEDULE_EDF;
	sch->ops = &schedule_mock_ops;
	list_item_append(&sch->list, &schedulers->list);

	struct comp_dev *first = calloc(sizeof(struct comp_dev), 1);
	struct sof_ipc_comp *first_comp = dev_comp(first);

	first_comp->id = 3;
	first_comp->pipeline_id = PIPELINE_ID_SAME;
	list_init(&first->bsink_list);
	list_init(&first->bsource_list);
	pipeline_connect_data->first = first;
	pipe->sched_comp = first;

	struct comp_dev *second = calloc(sizeof(struct comp_dev), 1);
	struct sof_ipc_comp *second_comp = dev_comp(second);

	second_comp->id = 4;
	second_comp->pipeline_id = PIPELINE_ID_DIFFERENT;
	list_init(&second->bsink_list);
	list_init(&second->bsource_list);
	pipeline_connect_data->second = second;

	struct comp_buffer *buffer = calloc(sizeof(struct comp_buffer), 1);

	buffer->source = first;
	buffer->sink = second;
	list_init(&buffer->sink_list);
	list_init(&buffer->source_list);
	pipeline_connect_data->b1 = buffer;

	struct comp_buffer *buffer_2 = calloc(sizeof(struct comp_buffer), 1);

	buffer_2->source = second;
	list_init(&buffer_2->sink_list);
	list_init(&buffer_2->source_list);
	pipeline_connect_data->b2 = buffer_2;

	pipeline_connect_data->p = *pipe;

	return pipeline_connect_data;
}
