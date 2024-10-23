/*-*- linux-c -*-*/

/*
 * ALSA <-> SOF PCM I/O plugin
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <sys/poll.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>

#include "plugin.h"
#include "common.h"

int plug_mq_cmd(struct plug_mq_desc *ipc, void *msg, size_t len, void *reply, size_t rlen)
{
	return plug_mq_cmd_tx_rx(ipc, ipc, msg, len, reply, rlen);
}

/*
 * Open an existing message queue using IPC object.
 */
int plug_mq_open(struct plug_mq_desc *ipc)
{
	/* now open new queue for Tx and Rx */
	ipc->mq = mq_open(ipc->queue_name,  O_RDWR);
	if (ipc->mq < 0) {
	//	SNDERR("failed to open IPC queue %s: %s\n",
	//		ipc->queue_name, strerror(errno));
		return -errno;
	}

	return 0;
}

/*
 * Open an existing semaphore using lock object.
 */
int plug_lock_open(struct plug_sem_desc *lock)
{
	lock->sem = sem_open(lock->name, O_RDWR);
	if (lock->sem == SEM_FAILED) {
		SNDERR("failed to open semaphore %s: %s\n", lock->name, strerror(errno));
		return -errno;
	}

	return 0;
}

#define itemsize(type, member) sizeof(((type *)0)->member)

static int parse_conf_long(snd_config_t *cfg, void *obj, size_t size)
{
	long val;

	if (snd_config_get_integer(cfg, &val) < 0)
		return -EINVAL;

	*((long *)obj) = val;
	return 0;
}

static int parse_conf_str(snd_config_t *cfg, void *obj, size_t size)
{
	const char *id;

	if (snd_config_get_id(cfg, &id) < 0)
		return -EINVAL;
	strncpy(obj, id, size);

	return 0;
}

static int parse_conf_format(snd_config_t *cfg, void *obj, size_t size)
{
	const char *id;

	if (snd_config_get_string(cfg, &id) < 0)
		return -EINVAL;

	if (!strcmp("S16_LE", id)) {
		*((long *)obj) = SND_PCM_FORMAT_S16_LE;
		return 0;
	}
	if (!strcmp("S32_LE", id)) {
		*((long *)obj) = SND_PCM_FORMAT_S32_LE;
		return 0;
	}
	if (!strcmp("S24_4LE", id)) {
		*((long *)obj) = SND_PCM_FORMAT_S24_LE;
		return 0;
	}
	if (!strcmp("FLOAT", id)) {
		*((long *)obj) = SND_PCM_FORMAT_FLOAT_LE;
		return 0;
	}

	/* not found */
	SNDERR("error: cant find format: %s", id);
	return -EINVAL;
}

struct config_item {
	char *name;
	size_t size;
	size_t offset;
	int (*copy)(snd_config_t *cfg, void *obj, size_t size);
};

struct config_item config_items[] = {
	{"name", itemsize(struct plug_config, name),
		offsetof(struct plug_config, name), parse_conf_str},
	{"rate", itemsize(struct plug_config, rate),
		offsetof(struct plug_config, rate), parse_conf_long},
	{"format", itemsize(struct plug_config, format),
		offsetof(struct plug_config, format), parse_conf_format},
	{"channels", itemsize(struct plug_config, channels),
		offsetof(struct plug_config, channels), parse_conf_long},
	{"period_time", itemsize(struct plug_config, period_time),
		offsetof(struct plug_config, period_time), parse_conf_long},
	{"period_frames", itemsize(struct plug_config, period_frames),
		offsetof(struct plug_config, period_frames), parse_conf_long},
	{"buffer_time", itemsize(struct plug_config, buffer_time),
		offsetof(struct plug_config, buffer_time), parse_conf_long},
	{"buffer_frames", itemsize(struct plug_config, buffer_frames),
		offsetof(struct plug_config, buffer_frames), parse_conf_long},
};

static int parse_item(snd_config_t *cfg, const char *id, struct plug_config *dest_cfg)
{
	void *dest = dest_cfg;
	int i;

	for (i = 0; i < ARRAY_SIZE(config_items); i++) {
		/* does ID match */
		if (strcmp(id, config_items[i].name))
			continue;

		/* now get the value */
		return config_items[i].copy(cfg, dest + config_items[i].offset,
					    config_items[i].size);
	}

	/* not found - non fatal */
	return 0;
}

static int parse_slave_configs(snd_sof_plug_t *plug, snd_config_t *n)
{
	snd_config_iterator_t si1, si2, snext1, snext2;
	struct plug_config *config;
	const char *id;

	fprintf(stdout, "Parsing ALSA conf for configs\n");

	snd_config_for_each(si1, snext1, n) {
		snd_config_t *sn1 = snd_config_iterator_entry(si1);

		config = &plug->config[plug->num_configs];

		/* get config name */
		if (parse_item(sn1, "name", config) < 0) {
			SNDERR("error: cant find config name");
			return -EINVAL;
		}

		/* now get item values in each config */
		snd_config_for_each(si2, snext2, sn1) {
			snd_config_t *sn2 = snd_config_iterator_entry(si2);

			if (snd_config_get_id(sn2, &id) < 0)
				continue;

			if (parse_item(sn2, id, config) < 0) {
				SNDERR("error: malformed config:  %s", id);
				return -EINVAL;
			}
		}

		fprintf(stdout, " config %d: %s\n", plug->num_configs,
			config->name);

		/* next config */
		plug->num_configs++;
		if (plug->num_configs >= PLUG_MAX_CONFIG) {
			SNDERR("error: too many configs");
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * Parse the client cmdline. Format is
 * tplg:pcm:card:dev:config[dai_pipe:card:dev:config]...]
 */
static int parse_client_cmdline(snd_sof_plug_t *plug, char *cmdline, bool just_tplg)
{
	struct plug_cmdline_item *cmd_item;
	char *tplg, *next, *card, *dev, *config, *pcm;
	char *tplg_path = getenv("SOF_PLUGIN_TOPOLOGY_PATH");
	char tplg_file[128];
	int ret;
	int i;

	if (!tplg_path) {
		SNDERR("Invalid topology path. Please set the SOF_PLUGIN_TOPOLOGY_PATH env variable\n");
		return -EINVAL;
	}

	/* get topology file */
	tplg = strtok_r(cmdline, ":", &next);
	if (!tplg) {
		SNDERR("invalid cmdline, cant find topology %s", cmdline);
		return -EINVAL;
	}

	/* now convert to filename and add the topology path */
	ret = snprintf(tplg_file, sizeof(tplg_file), "%ssof-%s.tplg", tplg_path, tplg);
	if (ret < 0) {
		SNDERR("invalid cmdline topology file %s", tplg);
		return -EINVAL;
	}
	plug->tplg_file = strdup(tplg_file);
	if (!plug->tplg_file)
		return -ENOMEM;

	if (just_tplg)
		return 0;

	/* get PCM ID */
	pcm = strtok_r(next, ":", &next);
	if (!pcm) {
		SNDERR("invalid cmdline, cant find PCM %s", pcm);
		return -EINVAL;
	}
	plug->pcm_id = atoi(pcm);

	fprintf(stdout, "Parsing cmd line\n");

	cmd_item = &plug->cmdline[plug->num_cmdline];
	card = strtok_r(next, ":", &next);
	/* card/dev names and config are all optional */
	if (!card) {
		strncpy(cmd_item->card_name, "default", sizeof(cmd_item->card_name));
		strncpy(cmd_item->dev_name, "default", sizeof(cmd_item->dev_name));
		fprintf(stdout, "no config name provided, will use hw_params\n");
	} else {
		strncpy(cmd_item->card_name, card, sizeof(cmd_item->card_name));
		dev = strtok_r(next, ":", &next);
		/* dev name must be provided along with card name */
		if (!dev) {
			SNDERR("Invalid dev name\n");
			return -EINVAL;
		}
		strncpy(cmd_item->dev_name, dev, sizeof(cmd_item->dev_name));
		config = strtok_r(next, ":", &next);
		/* config name is optional */
		if (!config)
			fprintf(stdout, "no config name provided, will use hw_params\n");
		else
			strncpy(cmd_item->config_name, config, sizeof(cmd_item->config_name));
	}

	cmd_item->pcm = atoi(pcm);

	/*
	 * dev name is special, we cant use "," in the command line
	 * so need to replace it with a "." and then later change it
	 * back to ","
	 */
	for (i = 0; i < sizeof(cmd_item->dev_name); i++) {
		if (cmd_item->dev_name[i] != '.')
			continue;
		cmd_item->dev_name[i] = ',';
		break;
	}

	fprintf(stdout, " cmd %d: for pcm %d uses %s with PCM %s:%s\n",
		plug->num_cmdline, cmd_item->pcm, cmd_item->config_name,
		cmd_item->card_name, cmd_item->dev_name);

	plug->num_cmdline++;

	printf("plug: topology file %s with pipe %ld\n", plug->tplg_file, plug->tplg_pipeline);
	return 0;
}

/*
 * Parse the ALSA conf for the SOF plugin and construct the command line options
 * to be passed into the SOF pipe executable.
 * TODO: verify all args
 * TODO: validate all args.
 * TODO: contruct sof pipe cmd line.
 */
int plug_parse_conf(snd_sof_plug_t *plug, const char *name, snd_config_t *root,
		    snd_config_t *conf, bool just_tplg)
{
	snd_config_iterator_t i, next;
	const char *tplg = NULL;

	/*
	 * The topology filename and topology PCM need to be passed in.
	 * i.e. aplay -Dsof:tplg:pcm:[card:dev:config]...]
	 */
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;

		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* dont care */
		if (strcmp(id, "comment") == 0 || strcmp(id, "type") == 0 ||
		    strcmp(id, "hint") == 0)
			continue;

		/* client command line topology */
		if (strcmp(id, "tplg") == 0) {
			if (snd_config_get_string(n, &tplg) < 0) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			} else if (!*tplg) {
				tplg = NULL;
			}
			continue;
		}

		/* topology PCM configurations */
		if (strcmp(id, "config") == 0) {
			if (parse_slave_configs(plug, n))
				return -EINVAL;
			continue;
		}

		/* not fatal - carry on and verify later */
		SNDERR("Unknown field %s", id);
	}

	/* verify mandatory inputs are specified */
	if (!tplg) {
		SNDERR("Missing topology topology");
		return -EINVAL;
	}

	/* parse the client command line */
	if (parse_client_cmdline(plug, (char *)tplg, just_tplg)) {
		SNDERR("invalid sof cmd line");
		return -EINVAL;
	}

	return 0;
}
