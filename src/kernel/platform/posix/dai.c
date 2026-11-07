// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2022 Google LLC.  All rights reserved.
// Author: Andy Ross <andyross@google.com>
#include <sof/lib/dai-legacy.h>

#define NUM_DAI_TYPES 12
#define DAIS_PER_TYPE 2

uint8_t useless_sum;

static int pdai_set_config(struct dai *dai, struct ipc_config_dai *config,
			   const void *spec_config)
{
	for (int i = 0; config && i < sizeof(*config); i++)
		useless_sum += ((uint8_t *)config)[i];
	return 0;
}

static int pdai_trigger(struct dai *dai, int cmd, int direction)
{
	return 0;
}

static int pdai_get_hw_params(struct dai *dai,
			      struct sof_ipc_stream_params *params, int dir)
{
	// FIXME: this is a polymorphic struct with extra
	// type-specific data at the end, may need to do this more
	// intelligently.
	memset(params, 0, sizeof(*params));
	return 0;
}

static int pdai_hw_params(struct dai *dai, struct sof_ipc_stream_params *params)
{
	for (int i = 0; params && i < sizeof(*params); i++)
		useless_sum += ((uint8_t *)params)[i];
	return 0;
}

static int pdai_get_handshake(struct dai *dai, int direction, int stream_id)
{
	return 0;
}

static int pdai_get_fifo(struct dai *dai, int direction, int stream_id)
{
	return 0;
}

static int pdai_probe(struct dai *dai)
{
	return 0;
}

static int pdai_remove(struct dai *dai)
{
	return 0;
}

static uint32_t pdai_get_init_delay_ms(struct dai *dai)
{
	return 0;
}

static int pdai_get_fifo_depth(struct dai *dai, int direction)
{
	return 0;
}

static void pdai_copy(struct dai *dai)
{
}

const struct dai_ops posix_dai_ops = {
	.set_config = pdai_set_config,
	.trigger = pdai_trigger,
	.get_hw_params = pdai_get_hw_params,
	.hw_params = pdai_hw_params,
	.get_handshake = pdai_get_handshake,
	.get_fifo = pdai_get_fifo,
	.probe = pdai_probe,
	.remove = pdai_remove,
	.get_init_delay_ms = pdai_get_init_delay_ms,
	.get_fifo_depth = pdai_get_fifo_depth,
	.copy = pdai_copy,
};

static struct dai_type_info dai_types[NUM_DAI_TYPES];
static struct dai_driver dai_drivers[NUM_DAI_TYPES];
static struct dai dais[NUM_DAI_TYPES][DAIS_PER_TYPE];

static const struct dai_info posix_dai_info = {
	.dai_type_array = dai_types,
	.num_dai_types = ARRAY_SIZE(dai_types),
};

void posix_dai_init(struct sof *sof)
{
	for (int type = 0; type < ARRAY_SIZE(dai_types); type++) {
		dai_types[type].type = type;
		dai_types[type].dai_array = &dais[type][0];
		dai_types[type].num_dais = DAIS_PER_TYPE;

		dai_drivers[type].type = type;
		//dai_drivers[type].uid = ???;
		//dai_drivers[type].tctx = ???;
		dai_drivers[type].dma_caps = BIT(0);
		dai_drivers[type].dma_dev = BIT(0);
		dai_drivers[type].ops = posix_dai_ops;
		//dai_drivers[type].ts_ops = ???;

		for (int i = 0; i < DAIS_PER_TYPE; i++) {
			dais[type][i].index = i;
			dais[type][i].drv = &dai_drivers[type];
		}
	}

	sof->dai_info = &posix_dai_info;
}
