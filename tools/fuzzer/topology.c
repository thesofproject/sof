// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

/* Topology loader to set up components and pipeline */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ipc/topology.h>
#include <ipc/stream.h>
#include <sof/common.h>
#include <tplg_parser/topology.h>

#include "../fuzzer/fuzzer.h"

const struct sof_dai_types sof_dais[] = {
	{"SSP", SOF_DAI_INTEL_SSP},
	{"HDA", SOF_DAI_INTEL_HDA},
	{"DMIC", SOF_DAI_INTEL_DMIC},
};

/* find dai type */
enum sof_ipc_dai_type find_dai(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sof_dais); i++) {
		if (strcmp(name, sof_dais[i].name) == 0)
			return sof_dais[i].type;
	}

	return SOF_DAI_INTEL_NONE;
}

void register_comp(int comp_type, struct sof_ipc_comp_ext *comp_ext) {}

int find_widget(struct comp_info *temp_comp_list, int count, char *name)
{
	int i;

	for (i = 0; i < count; i++) {
		if (!strcmp(temp_comp_list[i].name, name))
			return temp_comp_list[i].id;
	}

	return -EINVAL;
}

int complete_pipeline(struct fuzz *fuzzer, uint32_t comp_id)
{
	struct sof_ipc_pipe_ready ready;
	struct sof_ipc_reply r;
	int ret;

	fprintf(stdout, "tplg: complete pipeline id %d\n", comp_id);

	ready.hdr.size = sizeof(ready);
	ready.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_PIPE_COMPLETE;
	ready.comp_id = comp_id;

	/* configure fuzzer msg */
	fuzzer->msg.header = ready.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &ready, ready.hdr.size);
	fuzzer->msg.msg_size = sizeof(ready);
	fuzzer->msg.reply_size = sizeof(r);

	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		return ret;

	return 1;
}

/* load pipeline graph DAPM widget*/
static int load_graph(void *dev, struct comp_info *temp_comp_list,
		      int count, int num_comps, int pipeline_id)
{
	struct sof_ipc_pipe_comp_connect connection;
	struct fuzz *fuzzer = (struct fuzz *)dev;
	struct sof_ipc_reply r;
	char pipeline_string[DEBUG_MSG_LEN];
	int ret = 0;
	int i;

	for (i = 0; i < count; i++) {
		ret = tplg_load_graph(num_comps, pipeline_id, temp_comp_list,
				      pipeline_string, &connection,
				      fuzzer->tplg_file, i, count);
		if (ret < 0)
			return ret;

		/* configure fuzzer msg */
		fuzzer->msg.header = connection.hdr.cmd;
		memcpy(fuzzer->msg.msg_data, &connection, connection.hdr.size);
		fuzzer->msg.msg_size = sizeof(connection);
		fuzzer->msg.reply_size = sizeof(r);

		ret = fuzzer_send_msg(fuzzer);
		if (ret < 0)
			fprintf(stderr, "error: message tx failed\n");
	}

	return ret;
}

/* load buffer DAPM widget */
int load_buffer(void *dev, int comp_id, int pipeline_id,
		struct snd_soc_tplg_dapm_widget *widget)
{
	struct sof_ipc_buffer buffer;
	struct fuzz *fuzzer = (struct fuzz *)dev;
	struct sof_ipc_comp_reply r;
	int ret;

	ret = tplg_load_buffer(comp_id, pipeline_id, widget->priv.size, &buffer,
			       fuzzer->tplg_file);
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = buffer.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &buffer, buffer.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(buffer);
	fuzzer->msg.reply_size = sizeof(r);

	/* load volume component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return 0;
}

/* load pcm component */
static int load_pcm(void *dev, int comp_id, int pipeline_id, int size, int dir)
{
	struct fuzz *fuzzer = (struct fuzz *)dev;
	struct sof_ipc_comp_host host;
	struct sof_ipc_comp_reply r;
	int ret;

	ret = tplg_load_pcm(comp_id, pipeline_id, size, dir, &host,
			    fuzzer->tplg_file);
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = host.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &host, host.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(host);
	fuzzer->msg.reply_size = sizeof(r);

	/* load volume component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");
	return 0;
}

int load_aif_in_out(void *dev, int comp_id, int pipeline_id,
		    struct snd_soc_tplg_dapm_widget *widget, int dir, void *tp)
{
	return load_pcm(dev, comp_id, pipeline_id, widget->priv.size, dir);
}

/* load dai component */
static int load_dai(struct fuzz *fuzzer, int comp_id, int pipeline_id,
		    int size)
{
	struct sof_ipc_comp_dai comp_dai;
	struct sof_ipc_comp_reply r;
	int ret;

	ret = tplg_load_dai(comp_id, pipeline_id, size, &comp_dai,
			    fuzzer->tplg_file);
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = comp_dai.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &comp_dai, comp_dai.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(comp_dai);
	fuzzer->msg.reply_size = sizeof(r);

	/* load volume component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return 0;
}

int load_dai_in_out(void *dev, int comp_id, int pipeline_id,
		    struct snd_soc_tplg_dapm_widget *widget, int dir, void *tp)
{
	return load_dai(dev, comp_id, pipeline_id, widget->priv.size);
}

/* load pda dapm widget */
int load_pga(void *dev, int comp_id, int pipeline_id,
	     struct snd_soc_tplg_dapm_widget *widget)
{
	struct fuzz *fuzzer = (struct fuzz *)dev;
	struct sof_ipc_comp_volume volume;
	struct sof_ipc_comp_reply r;
	int ret = 0;

	ret = tplg_load_pga(comp_id, pipeline_id, widget->priv.size, &volume,
			    fuzzer->tplg_file);
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = volume.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &volume, volume.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(volume);
	fuzzer->msg.reply_size = sizeof(r);

	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return 0;
}

/* load scheduler dapm widget */
int load_pipeline(void *dev, int comp_id, int pipeline_id,
		  struct snd_soc_tplg_dapm_widget *widget, int sched_id)
{
	struct sof_ipc_pipe_new pipeline;
	struct fuzz *fuzzer = (struct fuzz *)dev;
	struct sof_ipc_comp_reply r;
	int ret;

	ret = tplg_load_pipeline(comp_id, pipeline_id, widget->priv.size,
				 &pipeline, fuzzer->tplg_file);
	if (ret < 0)
		return ret;

	pipeline.sched_id = sched_id;

	/* configure fuzzer msg */
	fuzzer->msg.header = pipeline.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &pipeline, pipeline.hdr.size);
	fuzzer->msg.msg_size = sizeof(pipeline);
	fuzzer->msg.reply_size = sizeof(r);

	/* load volume component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return 0;
}

/* load src dapm widget */
int load_src(void *dev, int comp_id, int pipeline_id,
	     struct snd_soc_tplg_dapm_widget *widget, void *params)
{
	struct fuzz *fuzzer = (struct fuzz *)dev;
	struct sof_ipc_comp_src src = {0};
	struct sof_ipc_comp_reply r;
	int ret = 0;

	ret = tplg_load_src(comp_id, pipeline_id, widget->priv.size, &src,
			    fuzzer->tplg_file);
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = src.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &src, src.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(src);
	fuzzer->msg.reply_size = sizeof(r);

	/* load volume component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return ret;
}

/* load asrc dapm widget */
int load_asrc(void *dev, int comp_id, int pipeline_id,
	      struct snd_soc_tplg_dapm_widget *widget, void *params)
{
	struct fuzz *fuzzer = (struct fuzz *)dev;
	struct sof_ipc_comp_asrc asrc = {0};
	struct sof_ipc_comp_reply r;
	int ret = 0;

	ret = tplg_load_asrc(comp_id, pipeline_id, widget->priv.size, &asrc,
			     fuzzer->tplg_file);
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = asrc.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &asrc, asrc.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(asrc);
	fuzzer->msg.reply_size = sizeof(r);

	/* load asrc component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return ret;
}

/* load mixer dapm widget */
int load_mixer(void *dev, int comp_id, int pipeline_id,
	       struct snd_soc_tplg_dapm_widget *widget)
{
	struct fuzz *fuzzer = (struct fuzz *)dev;
	struct sof_ipc_comp_mixer mixer = {0};
	struct sof_ipc_comp_reply r;
	int ret = 0;

	ret = tplg_load_mixer(comp_id, pipeline_id, widget->priv.size, &mixer,
			      fuzzer->tplg_file);
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = mixer.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &mixer, mixer.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(mixer);
	fuzzer->msg.reply_size = sizeof(r);

	/* load volume component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return ret;
}

/* load effect dapm widget */
int load_process(void *dev, int comp_id, int pipeline_id,
		 struct snd_soc_tplg_dapm_widget *widget)
{
	return -EINVAL; /* Not implemented */
}

/* parse topology file and set up pipeline */
int parse_tplg(struct fuzz *fuzzer, char *tplg_filename)
{
	struct snd_soc_tplg_hdr *hdr;

	struct comp_info *temp_comp_list = NULL;
	char message[DEBUG_MSG_LEN];
	int next_comp_id = 0, num_comps = 0;
	int i, ret = 0;
	size_t file_size, size;
	int sched_id;

	/* open topology file */
	fuzzer->tplg_file = fopen(tplg_filename, "rb");
	if (!fuzzer->tplg_file) {
		fprintf(stderr, "error: opening topology file %s\n",
			tplg_filename);
		return -errno;
	}

	/* file size */
	fseek(fuzzer->tplg_file, 0, SEEK_END);
	file_size = ftell(fuzzer->tplg_file);
	fseek(fuzzer->tplg_file, 0, SEEK_SET);

	/* allocate memory */
	size = sizeof(struct snd_soc_tplg_hdr);
	hdr = (struct snd_soc_tplg_hdr *)malloc(size);
	if (!hdr) {
		fprintf(stderr, "error: mem alloc\n");
		return -EINVAL;
	}

	fprintf(stdout, "debug: %s", "topology parsing start\n");

	while (1) {
		/* read topology header */
		ret = fread(hdr, sizeof(struct snd_soc_tplg_hdr), 1,
			    fuzzer->tplg_file);
		if (ret != 1)
			return -EINVAL;

		sprintf(message, "type: %x, size: 0x%x count: %d index: %d\n",
			hdr->type, hdr->payload_size, hdr->count, hdr->index);
		fprintf(stdout, "debug %s", message);

		/* parse header and load the next block based on type */
		switch (hdr->type) {
		/* load dapm widget */
		case SND_SOC_TPLG_TYPE_DAPM_WIDGET:
			sprintf(message, "number of DAPM widgets %d\n",
				hdr->count);
			fprintf(stdout, "debug %s\n", message);

			num_comps += hdr->count;
			size = sizeof(struct comp_info) * num_comps;
			temp_comp_list = (struct comp_info *)
					 realloc(temp_comp_list, size);

			for (i = (num_comps - hdr->count); i < num_comps; i++)
				ret = load_widget(fuzzer, FUZZER_DEV,
						  temp_comp_list,
						  next_comp_id++, i,
						  hdr->index, NULL, &sched_id,
						  fuzzer->tplg_file);
				if (ret < 0) {
					fprintf(stderr, "error: loading widget\n");
					goto finish;
				}
			break;

		/* set up component connections from pipeline graph */
		case SND_SOC_TPLG_TYPE_DAPM_GRAPH:
			if (load_graph(fuzzer, temp_comp_list, hdr->count,
				       num_comps, hdr->index) < 0) {
				fprintf(stderr, "error: pipeline graph\n");
				return -EINVAL;
			}
			if (ftell(fuzzer->tplg_file) == file_size)
				goto finish;
			break;
		default:
			fseek(fuzzer->tplg_file, hdr->payload_size, SEEK_CUR);
			if (ftell(fuzzer->tplg_file) == file_size)
				goto finish;
			break;
		}
	}
finish:
	/* pipeline complete after pipeline connections are established */
	for (i = 0; i < num_comps; i++)
		if (temp_comp_list[i].type == SND_SOC_TPLG_DAPM_SCHEDULER)
			complete_pipeline(fuzzer, temp_comp_list[i].id);

	fprintf(stdout, "debug: %s", "topology parsing end\n");

	/* free all data */
	free(hdr);

	for (i = 0; i < num_comps; i++)
		free(temp_comp_list[i].name);

	free(temp_comp_list);
	fclose(fuzzer->tplg_file);
	return 0;
}
