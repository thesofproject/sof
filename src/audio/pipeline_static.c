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
 *         Keyon Jie <yang.jie@linux.intel.comel.com>
 *
 * Static pipeline definition. This will be the default platform pipeline
 * definition if no pipeline is specified by driver topology.
 */

#include <stdint.h>
#include <stddef.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/dai.h>
#include <reef/ipc.h>
#include <platform/clk.h>
#include <platform/platform.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>

/* convenience component UUIDs and descriptors */
#define SPIPE_MIXER {COMP_TYPE_MIXER, 0, 0}
#define SPIPE_MUX {COMP_TYPE_MUX, 0, 0}
#define SPIPE_VOLUME(xindex) {COMP_TYPE_VOLUME, 0, 0}
#define SPIPE_SWITCH {COMP_TYPE_SWITCH, 0, 0}
#define SPIPE_DAI_SSP(xindex) {COMP_TYPE_DAI_SSP, xindex, 0}
#define SPIPE_HOST(xindex) {COMP_TYPE_HOST, xindex, 0}

/* convenience buffer descriptors */
#define SPIPE_BUFFER(xsize, xisize, xinum, xiirq, xosize, xonum, xoirq) \
	{.buffer = {.size = xsize, \
	.sink_period = {.size = xisize, .number = xinum, .no_irq = xiirq}, \
	.source_period = {.size = xosize, .number = xonum, .no_irq = xoirq},},}

/* Host facing buffer */
#define SPIPE_HOST_BUF \
	SPIPE_BUFFER(PLAT_HOST_PERSIZE * PLAT_HOST_PERIODS, \
		PLAT_HOST_PERSIZE, PLAT_HOST_PERIODS, 0, \
		PLAT_HOST_PERSIZE, PLAT_HOST_PERIODS, 0)

/* Device facing buffer */
#define SPIPE_DEV_BUF \
	SPIPE_BUFFER(PLAT_DEV_PERSIZE * PLAT_DEV_PERIODS, \
		PLAT_DEV_PERSIZE, PLAT_DEV_PERIODS, 0, \
		PLAT_DEV_PERSIZE, PLAT_DEV_PERIODS, 0)

struct spipe_comp {
	uint32_t type;
	uint32_t index;
	uint32_t id;
};

struct spipe_buffer {
	struct buffer_desc buffer;
	uint32_t id;
};

/* static link between components using UUIDs and IDs */
struct spipe_link {
	struct spipe_comp *source;
	struct spipe_buffer *buffer;
	struct spipe_comp *sink;
};

/* playback components */
static struct spipe_comp pipe_play_comps[] = {
	SPIPE_HOST(0),		/* ID = 0 */
	SPIPE_VOLUME(0),	/* ID = 1 */
	SPIPE_HOST(1),		/* ID = 2 */
	SPIPE_VOLUME(1),	/* ID = 3 */
	SPIPE_MIXER,		/* ID = 4 */
	SPIPE_VOLUME(2),	/* ID = 5 */
	SPIPE_DAI_SSP(PLATFORM_SSP_PORT),	/* ID = 6 */
};

/* capture components */
static struct spipe_comp pipe_capt_comps[] = {
	SPIPE_DAI_SSP(PLATFORM_SSP_PORT),	/* ID = 7 */
	SPIPE_VOLUME(3),	/* ID = 8 */
	SPIPE_HOST(2),		/* ID = 9 */
};

/* buffers used in pipeline */
static struct spipe_buffer pipe_buffers[] = {
	SPIPE_HOST_BUF,	/* B0 */
	SPIPE_HOST_BUF,	/* B1 */
	SPIPE_HOST_BUF,	/* B2 */
	SPIPE_HOST_BUF,	/* B3 */
	SPIPE_HOST_BUF,	/* B4 */
	SPIPE_DEV_BUF,	/* B5 */
	SPIPE_DEV_BUF,	/* B6 */
	SPIPE_HOST_BUF, /* B7 */
};

/*
 * Two PCMs mixed into single SSP output. SSP port is set in platform.h
 *
 * host PCM0(0) ---> volume(1) ---+
 *                                |mixer(4) --> volume(5) ---> SSPx(6)
 * host PCM1(2) ---> volume(3) ---+
 *
 * host PCM0(9) <--- volume(8) <--- SSPx(7)
 */
static struct spipe_link pipe_play[] = {
	{&pipe_play_comps[0], &pipe_buffers[0], &pipe_play_comps[1]},
	{&pipe_play_comps[2], &pipe_buffers[1], &pipe_play_comps[3]},
	{&pipe_play_comps[1], &pipe_buffers[2], &pipe_play_comps[4]},
	{&pipe_play_comps[3], &pipe_buffers[3], &pipe_play_comps[4]},
	{&pipe_play_comps[4], &pipe_buffers[4], &pipe_play_comps[5]},
	{&pipe_play_comps[5], &pipe_buffers[5], &pipe_play_comps[6]},
};

static struct spipe_link pipe_capture[] = {
	{&pipe_capt_comps[0], &pipe_buffers[6], &pipe_capt_comps[1]},
	{&pipe_capt_comps[1], &pipe_buffers[7], &pipe_capt_comps[2]},
};

/* static pipeline ID */
struct pipeline *pipeline_static;
uint32_t pipeline_id = 0;

struct pipeline *init_static_pipeline(void)
{
	int i, err;

	/* init system pipeline core */
	err = pipeline_init();
	if (err < 0)
		return NULL;

	/* create the pipeline */
	pipeline_static = pipeline_new(pipeline_id);
	if (pipeline_static == NULL)
		return NULL;

	/* create play back components in the pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_play_comps); i++) {
		pipe_play_comps[i].id = ipc_comp_new(pipeline_id,
			pipe_play_comps[i].type, pipe_play_comps[i].index,
			STREAM_DIRECTION_PLAYBACK);
		if (pipe_play_comps[i].id < 0)
			goto error;
	}

	/* create capture components in the pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_capt_comps); i++) {
		pipe_capt_comps[i].id = ipc_comp_new(pipeline_id,
			pipe_capt_comps[i].type, pipe_capt_comps[i].index,
			STREAM_DIRECTION_CAPTURE);
		if (pipe_capt_comps[i].id < 0)
			goto error;
	}

	/* create buffers in the pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_buffers); i++) {
		pipe_buffers[i].id = ipc_buffer_new(pipeline_id,
			&pipe_buffers[i].buffer);
		if (pipe_buffers[i].id < 0)
			goto error;
	}

	/* connect components on play back pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_play); i++) {
		/* add source -> sink */
		err = ipc_comp_connect(pipe_play[i].source->id,
			pipe_play[i].sink->id,
			pipe_play[i].buffer->id);
		if (err < 0)
			goto error;
	}

	/* create components on capture pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_capture); i++) {
		/* add source -> sink */
		err = ipc_comp_connect(pipe_capture[i].source->id,
			pipe_capture[i].sink->id,
			pipe_capture[i].buffer->id);
		if (err < 0)
			goto error;
	}

	/* pipeline now ready for params, prepare and cmds */
	return pipeline_static;

error:
	trace_pipe_error("ePS");
	pipeline_free(pipeline_static);
	return NULL;
}
