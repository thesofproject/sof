// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#ifndef __SOF_PLUGIN_PLUGIN_H__
#define __SOF_PLUGIN_PLUGIN_H__

#include <alsa/asoundlib.h>
#include "common.h"

#define PLUG_MAX_CONFIG	128

typedef struct snd_sof_plug {

	/* conf data */
	char *device;

	/* topology info */
	char *tplg_file;
	long tplg_pipeline; // HACK, use configs

	/* number of configurations in plugin conf */
	struct plug_config config[PLUG_MAX_CONFIG];
	int num_configs;

	/* command line arguments */
	struct plug_cmdline_item cmdline[PLUG_MAX_CONFIG];
	int num_cmdline;

	void *module_prv;	/* module private data */
} snd_sof_plug_t;

/*
 * ALSA Conf
 */
int sofplug_load_hook(snd_config_t *root, snd_config_t *config,
		      snd_config_t **dst, snd_config_t *private_data);

int plug_parse_conf(snd_sof_plug_t *plug, const char *name,
		    snd_config_t *root, snd_config_t *conf);

#endif
