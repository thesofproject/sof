// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021-2022 Intel Corporation. All rights reserved.
//
// Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/list.h>

#include <errno.h>

/*
 * Valid state transitions:
 * COMP_STATE_INIT		-> [COMP_TRIGGER_XRUN]		-> COMP_STATE_INIT
 * COMP_STATE_INIT		-> [COMP_TRIGGER_RESET]		-> COMP_STATE_READY
 * COMP_STATE_READY		-> [COMP_TRIGGER_RESET]		-> COMP_STATE_READY
 * COMP_STATE_READY		-> [COMP_TRIGGER_PREPARE]	-> COMP_STATE_PREPARE
 * COMP_STATE_SUSPEND		-> [COMP_TRIGGER_XRUN]		-> COMP_STATE_SUSPEND
 * COMP_STATE_SUSPEND		-> [COMP_TRIGGER_RESET]		-> COMP_STATE_READY
 * COMP_STATE_PREPARE		-> [COMP_TRIGGER_PRE_START]	-> COMP_STATE_PRE_ACTIVE
 * COMP_STATE_PREPARE		-> [COMP_TRIGGER_XRUN]		-> COMP_STATE_PREPARE
 * COMP_STATE_PREPARE		-> [COMP_TRIGGER_RESET]		-> COMP_STATE_READY
 * COMP_STATE_PREPARE		-> [COMP_TRIGGER_PREPARE]	-> COMP_STATE_PREPARE
 * COMP_STATE_PAUSED		-> [COMP_TRIGGER_PRE_RELEASE]	-> COMP_STATE_PRE_ACTIVE
 * COMP_STATE_PAUSED		-> [COMP_TRIGGER_STOP]		-> COMP_STATE_PREPARE
 * COMP_STATE_PAUSED		-> [COMP_TRIGGER_XRUN]		-> COMP_STATE_PAUSED
 * COMP_STATE_PAUSED		-> [COMP_TRIGGER_RESET]		-> COMP_STATE_READY
 * COMP_STATE_ACTIVE		-> [COMP_TRIGGER_STOP]		-> COMP_STATE_PREPARE
 * COMP_STATE_ACTIVE		-> [COMP_TRIGGER_XRUN]		-> COMP_STATE_ACTIVE
 * COMP_STATE_ACTIVE		-> [COMP_TRIGGER_PAUSE]		-> COMP_STATE_PAUSED
 * COMP_STATE_ACTIVE		-> [COMP_TRIGGER_RESET]		-> COMP_STATE_READY
 * COMP_STATE_PRE_ACTIVE	-> [COMP_TRIGGER_START]		-> COMP_STATE_ACTIVE
 * COMP_STATE_PRE_ACTIVE	-> [COMP_TRIGGER_RELEASE]	-> COMP_STATE_ACTIVE
 *
 * Target states
 * COMP_STATE_INIT		<- *init*
 * COMP_STATE_READY		<- [COMP_TRIGGER_RESET]		# PCM_FREE -> pipeline_reset()
 * COMP_STATE_PREPARE		<- [COMP_TRIGGER_STOP]
 * COMP_STATE_PREPARE		<- [COMP_TRIGGER_XRUN]
 * COMP_STATE_PREPARE		<- [COMP_TRIGGER_PREPARE]	# PCM_PARAMS -> pipeline_prepare()
 * <unused>			<- [COMP_TRIGGER_SUSPEND]
 */

static int source_status_count(struct comp_dev *dev, unsigned int status)
{
	struct comp_buffer *source;
	struct list_item *blist;
	int count = 0;

	/* count sources with state == status */
	list_for_item(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);
		comp_info(source->source, "source state %u", source->source->state);
		if (source->source->state == status)
			count++;
	}

	return count;
}

static int sink_status_count(struct comp_dev *dev, unsigned int status)
{
	struct comp_buffer *sink;
	struct list_item *blist;
	int count = 0;

	/* count sinks with state == status */
	list_for_item(blist, &dev->bsink_list) {
		sink = container_of(blist, struct comp_buffer, source_list);
		comp_info(sink->sink, "sink state %u", sink->sink->state);
		if (sink->sink->state == status)
			count++;
	}

	return count;
}

static unsigned int fork_status_count(struct comp_dev *dev,
				      unsigned int status)
{
	switch (dev_comp_type(dev)) {
	case SOF_COMP_MIXER:
	case SOF_COMP_MUX:
		return source_status_count(dev, status);
	case SOF_COMP_DEMUX:
		return sink_status_count(dev, status);
	default:
		return 0;
	}
}

static int fork_handle_stop(struct comp_dev *dev)
{
	switch (dev->state) {
	case COMP_STATE_ACTIVE:
		/* from flow: extended for multiple inputs */
		if (fork_status_count(dev, COMP_STATE_ACTIVE))
			return PPL_STATUS_PATH_STOP;

		if (fork_status_count(dev, COMP_STATE_PAUSED)) {
			/* only one active pipeline, but other pipelines are paused */
			dev->cmd_override = COMP_TRIGGER_PAUSE;
			dev->state = COMP_STATE_PAUSED;
			return 0;
		}

		dev->state = COMP_STATE_PREPARE;
		return 0;
	}

	return -EINVAL;
}

static int fork_handle_start(struct comp_dev *dev)
{
	switch (dev->state) {
	case COMP_STATE_PRE_ACTIVE:
		if (fork_status_count(dev, COMP_STATE_PAUSED)) {
			/* we were in PAUSED when a PRE_START arrived */
			dev->cmd_override = COMP_TRIGGER_RELEASE;
		}

		/* from flow */
		dev->state = COMP_STATE_ACTIVE;
		return 0;
	case COMP_STATE_ACTIVE:
		return PPL_STATUS_PATH_STOP;
	}

	return -EINVAL;
}

static int fork_handle_pause(struct comp_dev *dev)
{
	switch (dev->state) {
	case COMP_STATE_ACTIVE:
		/* from flow: extended for multiple inputs */
		if (fork_status_count(dev, COMP_STATE_ACTIVE) ||
		    fork_status_count(dev, COMP_STATE_PAUSED) > 1)
			/* Keep the current state */
			return PPL_STATUS_PATH_STOP;

		dev->state = COMP_STATE_PAUSED;
		return 0;
	}

	return -EINVAL;
}

static int fork_handle_release(struct comp_dev *dev)
{
	switch (dev->state) {
	case COMP_STATE_ACTIVE:
		/*
		 * the mixer and its pipeline didn't suspend - they have
		 * additional active source pipelines
		 */
		return PPL_STATUS_PATH_STOP;
	case COMP_STATE_PRE_ACTIVE:
		/* from flow */
		dev->state = COMP_STATE_ACTIVE;
		return 0;
	}

	return -EINVAL;
}

static int fork_handle_suspend(struct comp_dev *dev)
{
	return -EINVAL;
}

static int fork_handle_resume(struct comp_dev *dev)
{
	return -EINVAL;
}

static int fork_handle_reset(struct comp_dev *dev)
{
	switch (dev->state) {
	case COMP_STATE_INIT:
	case COMP_STATE_READY:
	case COMP_STATE_SUSPEND:
	case COMP_STATE_PAUSED:
	case COMP_STATE_PREPARE:
	case COMP_STATE_ACTIVE:
		dev->state = COMP_STATE_READY;
		return 0;
	}

	return -EINVAL;
}

static int fork_handle_prepare(struct comp_dev *dev)
{
	switch (dev->state) {
	case COMP_STATE_READY:
		dev->state = COMP_STATE_PREPARE;
		return 0;
	case COMP_STATE_PREPARE:
	case COMP_STATE_ACTIVE:
		/* other sources are active, stop propagation */
		return PPL_STATUS_PATH_STOP;
	}

	return -EINVAL;
}

static int fork_handle_xrun(struct comp_dev *dev)
{
	return -EINVAL;
}

static int fork_handle_pre_start(struct comp_dev *dev)
{
	switch (dev->state) {
	case COMP_STATE_PREPARE:
		/* from flow */
		dev->state = COMP_STATE_PRE_ACTIVE;
		return 0;
	case COMP_STATE_ACTIVE:
		/* other sources are active, stop propagation */
		return PPL_STATUS_PATH_STOP;
	case COMP_STATE_PAUSED:
		dev->state = COMP_STATE_PRE_ACTIVE;
		dev->cmd_override = COMP_TRIGGER_PRE_RELEASE;
		return 0;
	}

	return -EINVAL;
}

static int fork_handle_pre_release(struct comp_dev *dev)
{
	switch (dev->state) {
	case COMP_STATE_ACTIVE:
		/* other sources are active, stop propagation */
		return PPL_STATUS_PATH_STOP;
	case COMP_STATE_PAUSED:
		dev->state = COMP_STATE_PRE_ACTIVE;
		return 0;
	}

	return -EINVAL;
}

int fork_state_matrix(struct comp_dev *dev, int cmd)
{
	dev->cmd_override = -EINVAL;

	switch (cmd) {
	case COMP_TRIGGER_STOP:
		return fork_handle_stop(dev);
	case COMP_TRIGGER_START:
		return fork_handle_start(dev);
	case COMP_TRIGGER_PAUSE:
		return fork_handle_pause(dev);
	case COMP_TRIGGER_RELEASE:
		return fork_handle_release(dev);
	case COMP_TRIGGER_SUSPEND:
		return fork_handle_suspend(dev);
	case COMP_TRIGGER_RESUME:
		return fork_handle_resume(dev);
	case COMP_TRIGGER_RESET:
		return fork_handle_reset(dev);
	case COMP_TRIGGER_PREPARE:
		return fork_handle_prepare(dev);
	case COMP_TRIGGER_XRUN:
		return fork_handle_xrun(dev);
	case COMP_TRIGGER_PRE_START:
		return fork_handle_pre_start(dev);
	case COMP_TRIGGER_PRE_RELEASE:
		return fork_handle_pre_release(dev);
	}

	return -EINVAL;
}
