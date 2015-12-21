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

/* convenience component UUIDs */
#define SPIPE_MIXER(xid) \
	{COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_MIXER), xid}
#define SPIPE_MUX(xid) \
	{COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_MUX), xid}
#define SPIPE_VOLUME(xid) \
	{COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_VOLUME), xid}
#define SPIPE_SWITCH(xid) \
	{COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_SWITCH), xid}
#define SPIPE_DAI_SSP0(xid) \
	{COMP_UUID(COMP_VENDOR_INTEL, DAI_UUID_SSP0), xid}
#define SPIPE_HOST(xid) \
	{COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_HOST), xid}

/* static link between components using UUIDs and IDs */
struct spipe_link {
	struct comp_desc source;
	struct comp_desc comp;
	struct comp_desc sink;
};

/*
 * Straight through playback and capture pipes with simple volume.
 * 
 * host PCM0(0) ---> volume(1) ---> SSP0(2)
 * host PCM0(0) <--- volume(3) <--- SSP0(2)
 */
static struct spipe_link pipe_play0[] = {
	{SPIPE_HOST(0), SPIPE_VOLUME(1), SPIPE_DAI_SSP0(2)},
};

static struct spipe_link pipe_capture0[] = {
	{SPIPE_DAI_SSP0(2), SPIPE_VOLUME(3), SPIPE_HOST(0)},
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
int pipeline_static;

int init_static_pipeline(void)
{
	int pl, i, err;

	/* init system pipeline core */
	err = pipeline_init();
	if (err < 0)
		return err;

	/* create the pipeline */
	pl = pipeline_new();
	if (pl < 0)
		return pl;

	/* create playback components in the pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_play0); i++) {
		pipeline_comp_new(pl, &pipe_play0[i].source);
		pipeline_comp_new(pl, &pipe_play0[i].comp);
		pipeline_comp_new(pl, &pipe_play0[i].sink);
	}

	/* create capture components in the pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_play0); i++) {
		pipeline_comp_new(pl, &pipe_capture0[i].source);
		pipeline_comp_new(pl, &pipe_capture0[i].comp);
		pipeline_comp_new(pl, &pipe_capture0[i].sink);
	} 

	/* create components on playback pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_play0); i++) {
		/* add source -> comp */
		err = pipeline_comp_connect(pl, &pipe_play0[i].source,
			&pipe_play0[i].comp);
		if (err < 0)
			goto err;

		/* add comp -> sink */
		err = pipeline_comp_connect(pl, &pipe_play0[i].comp,
			&pipe_play0[i].sink);
		if (err < 0)
			goto err;
	}

	/* create components on capture pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_capture0); i++) {
		/* add source -> comp */
		err = pipeline_comp_connect(pl, &pipe_capture0[i].source,
			&pipe_capture0[i].comp);
		if (err < 0)
			goto err;

		/* add comp -> sink */
		err = pipeline_comp_connect(pl, &pipe_capture0[i].comp,
			&pipe_capture0[i].sink);
		if (err < 0)
			goto err;
	}

	/* pipeline now ready for params, prepare and cmds */
	pipeline_static = pl;
	return pl;

err:
	pipeline_free(pl);
	return err;
}
