// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>

#include <sof/audio/ipc-config.h>
#include "pipeline_connection_mocks.h"

extern struct schedulers *schedulers;

struct scheduler_ops schedule_mock_ops = {
	.schedule_task_free	= &schedule_task_mock_free,
};

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
	struct comp_ipc_config *first_comp = &first->ipc_config;

	first_comp->id = 3;
	first_comp->pipeline_id = PIPELINE_ID_SAME;
	list_init(&first->bsink_list);
	list_init(&first->bsource_list);
	pipeline_connect_data->first = first;
	pipe->sched_comp = first;

	struct comp_dev *second = calloc(sizeof(struct comp_dev), 1);
	struct comp_ipc_config *second_comp = &second->ipc_config;

	second_comp->id = 4;
	second_comp->pipeline_id = PIPELINE_ID_DIFFERENT;
	list_init(&second->bsink_list);
	list_init(&second->bsource_list);
	pipeline_connect_data->second = second;

	struct comp_buffer *buffer = calloc(sizeof(struct comp_buffer), 1);

	comp_buffer_set_source_component(buffer, first);
	comp_buffer_set_sink_component(buffer, second);
	comp_buffer_reset_sink_list(buffer);
	comp_buffer_reset_source_list(buffer);
	pipeline_connect_data->b1 = buffer;

	struct comp_buffer *buffer_2 = calloc(sizeof(struct comp_buffer), 1);

	comp_buffer_set_source_component(buffer_2, second);
	comp_buffer_reset_sink_list(buffer_2);
	comp_buffer_reset_source_list(buffer_2);
	pipeline_connect_data->b2 = buffer_2;

	pipeline_connect_data->p = *pipe;

	return pipeline_connect_data;
}
