/*
 * Copyright (c) 2018, Intel Corporation
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
 * Author: Jakub Dabek <jakub.dabek@linux.intel.com>
 */

#include "pipeline_connection_mocks.h"

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

	struct sof_ipc_pipe_new pipe_desc = {
		.frames_per_sched = 5,
		.pipeline_id = PIPELINE_ID_SAME };

	struct pipeline *pipe = calloc(sizeof(struct pipeline), 1);

	pipe->ipc_pipe = pipe_desc;
	pipe->status = COMP_STATE_INIT;

	struct comp_dev *first = calloc(sizeof(struct comp_dev), 1);

	first->comp.id = 3;
	first->comp.pipeline_id = PIPELINE_ID_SAME;
	list_init(&first->bsink_list);
	list_init(&first->bsource_list);
	pipeline_connect_data->first = first;
	pipe->sched_comp = first;

	struct comp_dev *second = calloc(sizeof(struct comp_dev), 1);

	second->comp.id = 4;
	second->comp.pipeline_id = PIPELINE_ID_DIFFERENT;
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
