/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/dai.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>

/* convenience component UUIDs and descriptors */
#define SPIPE_MIXER(xid) \
	{COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_MIXER), xid}
#define SPIPE_MUX(xid) \
	{COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_MUX), xid}
#define SPIPE_VOLUME(xid) \
	{COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_VOLUME), xid}
#define SPIPE_SWITCH(xid) \
	{COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_SWITCH), xid}
#define SPIPE_DAI_SSP0(xid, is_play) \
	{COMP_UUID(COMP_VENDOR_INTEL, COMP_TYPE_DAI_SSP), xid, is_play}
#define SPIPE_HOST(xid, is_play) \
	{COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_HOST), xid, is_play}

/* convenience buffer descriptors */
#define SPIPE_BUFFER(xsize, xisize, xinum, xiirq, xosize, xonum, xoirq) \
	{.size = xsize, \
		.sink_period = {.size = xisize, .number = xinum, .no_irq = xiirq}, \
		.source_period = {.size = xosize, .number = xonum, .no_irq = xoirq},}

/* Host facing buffer */
#define SPIPE_HOST_BUF \
	SPIPE_BUFFER(PLAT_HOST_PERSIZE * PLAT_HOST_PERIODS, \
		PLAT_HOST_PERSIZE * PLAT_HOST_PERIODS, PLAT_HOST_PERIODS, 0, \
		-1, -1, 0)

/* Device facing buffer */
#define SPIPE_DEV_BUF \
	SPIPE_BUFFER(PLAT_DEV_PERSIZE * PLAT_DEV_PERIODS, \
		-1, -1, 0, \
		PLAT_DEV_PERSIZE * PLAT_DEV_PERIODS, PLAT_DEV_PERIODS, 0)

/* static link between components using UUIDs and IDs */
struct spipe_link {
	struct comp_desc source;
	struct buffer_desc buffer;
	struct comp_desc sink;
};

/*
 * Straight through playback and capture pipes with simple volume.
 * 
 * host PCM0(0) ---> B0 ---> volume(1) ---> B1 ---> SSP0(2)
 * host PCM0(0) <--- B2 <--- volume(3) <--- B3 <--- SSP0(2)
 */

static struct comp_desc pipe0_comps[] = {
	SPIPE_HOST(0, 1),
	SPIPE_VOLUME(1),
	SPIPE_DAI_SSP0(2, 1),
	SPIPE_DAI_SSP0(2, 0),
	SPIPE_VOLUME(3),
	SPIPE_HOST(0, 0),
};

static struct spipe_link pipe_play0[] = {
	{SPIPE_HOST(0, 1), SPIPE_HOST_BUF, SPIPE_VOLUME(1)},
	{SPIPE_VOLUME(1), SPIPE_DEV_BUF, SPIPE_DAI_SSP0(2, 1)},
};


static struct spipe_link pipe_capture0[] = {
	{SPIPE_DAI_SSP0(2, 0), SPIPE_DEV_BUF, SPIPE_VOLUME(3)},
	{SPIPE_VOLUME(3), SPIPE_HOST_BUF, SPIPE_HOST(0, 0)},
};


#if 0
/* 
 * Two PCMs mixed into single SSP output.
 *
 * host PCM0(0) ---> volume(2) ---+
 *                                | mixer(4) --> volume(5) ---> SSP0(6)
 * host PCM1(1) ---> volume(3) ---+
 */
static struct spipe_link pipe_play1[] = {
	{SPIPE_HOST(0), SPIPE_VOLUME(2), SPIPE_MIXER(4)},
	{SPIPE_HOST(1), SPIPE_VOLUME(3), SPIPE_MIXER(4)},
	{SPIPE_MIXER(4), SPIPE_VOLUME(5), SPIPE_DAI_SSP0(6)},
};

/*
 * Default BDW pipelines with Linux FW Lite
 *
 * host PCM0(0) ---> volume(4) ---+
 *                                |
 * host PCM1(1) ---> volume(5) ---+ mixer(9) -+-> volume(10) ---> SSP0(11)
 *                                |           |
 * host PCM2(2) ---> volume(6) ---+           |
 *                                            |
 * host PCM3(3) <--- volume(7) <--------------+
 *
 * host PCM0(0) <--- volume(8) <--- SSP0(11)
 */
static struct spipe_link pipe_play2[] = {
	{SPIPE_HOST(0), SPIPE_VOLUME(4), SPIPE_MIXER(9)},
	{SPIPE_HOST(1), SPIPE_VOLUME(5), SPIPE_MIXER(9)},
	{SPIPE_HOST(2), SPIPE_VOLUME(6), SPIPE_MIXER(9)},
	{SPIPE_MIXER(9), SPIPE_VOLUME(10), SPIPE_DAI_SSP0(11)},
	{SPIPE_MIXER(9), SPIPE_VOLUME(7), SPIPE_HOST(3)},
};

static struct spipe_link pipe_capture2[] = {
	{SPIPE_DAI_SSP0(11), SPIPE_VOLUME(8), SPIPE_HOST(0)},
};
#endif

/* static pipeline ID */
struct pipeline *pipeline_static;

struct pipeline *init_static_pipeline(void)
{
	int i, err;

	trace_point(0x2010);

	/* init system pipeline core */
	err = pipeline_init();
	if (err < 0)
		return NULL;

	/* create the pipeline */
	pipeline_static = pipeline_new();
	if (pipeline_static < 0)
		return NULL;

	trace_point(0x2020);

	/* create components in the pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe0_comps); i++) {
		pipeline_comp_new(pipeline_static, &pipe0_comps[i]);
	}

	trace_point(0x2030);

	/* create components on playback pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_play0); i++) {
		/* add source -> sink */
		err = pipeline_comp_connect(pipeline_static, &pipe_play0[i].source,
			&pipe_play0[i].sink, &pipe_play0[i].buffer);
		if (err < 0)
			goto err;
	}

	trace_point(0x2040);

	/* create components on capture pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_capture0); i++) {
		/* add source -> sink */
		err = pipeline_comp_connect(pipeline_static, &pipe_capture0[i].source,
			&pipe_capture0[i].sink, &pipe_capture0[i].buffer);
		if (err < 0)
			goto err;
	}

	trace_point(0x2050);

	/* pipeline now ready for params, prepare and cmds */
	return pipeline_static;

err:
	trace_point(0x2060);
	pipeline_free(pipeline_static);
	return NULL;
}
