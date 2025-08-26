/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Jakub Dabek <jakub.dabek@linux.intel.com>
 */

#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/schedule/schedule.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#else
#include <stdlib.h>
#endif

#define PIPELINE_ID_SAME 3
#define PIPELINE_ID_DIFFERENT 4

struct pipeline_connect_data {
	struct pipeline p;
	struct comp_dev *first;
	struct comp_dev *second;
	struct comp_buffer *b1;
	struct comp_buffer *b2;
};

struct pipeline_connect_data *get_standard_connect_objects(void);
void free_standard_connect_objects(struct pipeline_connect_data *data);

void cleanup_test_data(struct pipeline_connect_data *data);

static inline int schedule_task_mock_free(void *data, struct task *task)
{
	task->state = SOF_TASK_STATE_FREE;
	task->ops.run = NULL;
	task->data = NULL;

	return 0;
}
