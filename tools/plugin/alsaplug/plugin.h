/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022-2023 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_PLUGIN_PLUGIN_H__
#define __SOF_PLUGIN_PLUGIN_H__

#include <alsa/asoundlib.h>
#include <sof/list.h>
#include "common.h"

#include <tplg_parser/topology.h>

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

	/* topology */
	struct tplg_context tplg;
	struct list_item widget_list;
	struct list_item route_list;
	struct list_item pcm_list;
	struct list_item pipeline_list;
	int instance_ids[SND_SOC_TPLG_DAPM_LAST];
	struct plug_mq_desc ipc_tx;
	struct plug_mq_desc ipc_rx;

	int pcm_id;
	struct tplg_pcm_info *pcm_info;

	snd_pcm_uframes_t period_size;

	void *module_prv;	/* module private data */
} snd_sof_plug_t;

/*
 * ALSA Conf
 */
int sofplug_load_hook(snd_config_t *root, snd_config_t *config,
		      snd_config_t **dst, snd_config_t *private_data);

int plug_parse_conf(snd_sof_plug_t *plug, const char *name,
		    snd_config_t *root, snd_config_t *conf);
int plug_parse_topology(snd_sof_plug_t *plug);
int plug_set_up_pipelines(snd_sof_plug_t *plug, int dir);
int plug_free_pipelines(snd_sof_plug_t *plug, struct tplg_pipeline_list *pipeline_list, int dir);
void plug_free_topology(snd_sof_plug_t *plug);

#endif
