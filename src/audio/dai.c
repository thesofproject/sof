/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/dai.h>
#include <reef/alloc.h>
#include <reef/stream.h>
#include <reef/audio/component.h>

static struct comp_dev *dai_new_ssp(uint32_t uuid, int id)
{
	struct comp_dev *dev;
	struct dai *ssp;

	ssp = dai_get(COMP_UUID(COMP_VENDOR_INTEL, id));
	if (ssp == NULL)
		return NULL;

	dev = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*dev));
	if (dev == NULL)
		return NULL;

	comp_set_drvdata(dev, ssp);
	dev->id = id;
	return dev;
}

static struct comp_dev *dai_new_hda(uint32_t uuid, int id)
{

	return 0;
}

static void dai_free(struct comp_dev *dev)
{
	rfree(RZONE_MODULE, RMOD_SYS, dev);
}

/* set component audio COMP paramters */
static int dai_params(struct comp_dev *dev, struct stream_params *params)
{

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int dai_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct dai *ssp = comp_get_drvdata(dev);

	/* most pipeline commands can be directly passed to DAI driver */
	switch (cmd) {
	case PIPELINE_CMD_DRAIN:
		break;
	default:
		return dai_trigger(ssp, cmd, dev->is_playback);
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int dai_copy(struct comp_dev *sink, struct comp_dev *source)
{

	return 0;
}

static struct comp_driver comp_dai_ssp = {
	.uuid	= COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_DAI_SSP),
	.ops	= {
		.new		= dai_new_ssp,
		.free		= dai_free,
		.params		= dai_params,
		.cmd		= dai_cmd,
		.copy		= dai_copy,
	},
	.caps	= {
		.source = {
			.formats	= STREAM_FORMAT_S16_LE,
			.min_rate	= 8000,
			.max_rate	= 192000,
			.min_channels	= 1,
			.max_channels	= 2,
		},
		.sink = {
			.formats	= STREAM_FORMAT_S16_LE,
			.min_rate	= 8000,
			.max_rate	= 192000,
			.min_channels	= 1,
			.max_channels	= 2,
		},
	},
};

static struct comp_driver comp_dai_hda = {
	.uuid	= COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_DAI_HDA),
	.ops	= {
		.new		= dai_new_hda,
		.free		= dai_free,
		.params		= dai_params,
		.cmd		= dai_cmd,
		.copy		= dai_copy,
	},
};

void sys_comp_dai_init(void)
{
	comp_register(&comp_dai_ssp);
	comp_register(&comp_dai_hda);
}

