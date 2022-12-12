// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>

/* Topology loader to set up components and pipeline */

#include <stdio.h>
#include <sys/poll.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <dlfcn.h>

#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

#include <alsa/asoundlib.h>
#include <alsa/control_external.h>
#include <alsa/pcm_external.h>

#include "pipe.h"

#define FILE_READ 0
#define FILE_WRITE 1

#define MAX_TPLG_OBJECT_SIZE	4096

//#include <sound/asound.h>

/* temporary - current MAXLEN is not define in UAPI header - fix pending */
#ifndef SNDRV_CTL_ELEM_ID_NAME_MAXLEN
#define SNDRV_CTL_ELEM_ID_NAME_MAXLEN	44
#endif
#include <alsa/sound/asoc.h>

static int plug_load_fileread(struct tplg_context *ctx,
			      struct sof_ipc_comp_file *fileread)
{
	struct snd_soc_tplg_vendor_array *array = &ctx->widget->priv.array[0];
	size_t total_array_size = 0;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	char uuid[UUID_SIZE];
	int ret;

	/* read vendor tokens */
	while (total_array_size < size) {

		if (!tplg_is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: filewrite array size mismatch for widget size %d\n",
				size);
			return -EINVAL;
		}

		/* parse comp tokens */
		ret = sof_parse_tokens(&fileread->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse comp tokens %d\n",
				size);
			return -EINVAL;
		}

		/* parse uuid token */
		ret = sof_parse_tokens(uuid, comp_ext_tokens,
				       ARRAY_SIZE(comp_ext_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse fileread uuid token %d\n", size);
			return -EINVAL;
		}


		total_array_size += array->size;
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

	/* configure fileread */
	fileread->mode = FILE_READ;
	fileread->comp.id = comp_id;

	/* use fileread comp as scheduling comp */
	fileread->comp.core = ctx->core_id;
	fileread->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	fileread->comp.hdr.size = sizeof(struct sof_ipc_comp_file) + UUID_SIZE;
	fileread->comp.type = SOF_COMP_FILEREAD;
	fileread->comp.pipeline_id = ctx->pipeline_id;
	fileread->comp.ext_data_length = UUID_SIZE;
	fileread->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	memcpy(fileread + 1, &uuid, UUID_SIZE);

	return 0;
}

/* load filewrite component */
static int plug_load_filewrite(struct tplg_context *ctx,
			       struct sof_ipc_comp_file *filewrite)
{
	struct snd_soc_tplg_vendor_array *array = &ctx->widget->priv.array[0];
	size_t total_array_size = 0;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	char uuid[UUID_SIZE];
	int ret;

	/* read vendor tokens */
	while (total_array_size < size) {

		if (!tplg_is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: filewrite array size mismatch\n");
			return -EINVAL;
		}

		ret = sof_parse_tokens(&filewrite->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse filewrite tokens %d\n",
				size);
			return -EINVAL;
		}

		/* parse uuid token */
		ret = sof_parse_tokens(uuid, comp_ext_tokens,
				       ARRAY_SIZE(comp_ext_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse mixer uuid token %d\n", size);
			return -EINVAL;
		}

		total_array_size += array->size;
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

	/* configure filewrite */
	filewrite->comp.core = ctx->core_id;
	filewrite->comp.id = comp_id;
	filewrite->mode = FILE_WRITE;
	filewrite->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	filewrite->comp.hdr.size = sizeof(struct sof_ipc_comp_file) + UUID_SIZE;
	filewrite->comp.type = SOF_COMP_FILEWRITE;
	filewrite->comp.pipeline_id = ctx->pipeline_id;
	filewrite->comp.ext_data_length = UUID_SIZE;
	filewrite->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	memcpy(filewrite + 1, &uuid, UUID_SIZE);

	return 0;
}

/* load fileread component */
static int load_fileread(struct sof_pipe *sp, struct tplg_context *ctx, int dir)
{
	struct sof_ipc_comp_file *fileread;
	int ret;

	fileread = calloc(1, sizeof(struct sof_ipc_comp_file)  + UUID_SIZE);
	if (!fileread)
		return -ENOMEM;

	ret = plug_load_fileread(ctx, fileread);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx, ctx->widget->num_kcontrols, NULL, 0, NULL) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	/* use fileread comp as scheduling comp */
	ctx->sched_id = ctx->comp_id;

	fileread->direction = dir;

	/* Set type depending on direction */
	fileread->comp.type = (dir == SOF_IPC_STREAM_PLAYBACK) ?
		SOF_COMP_HOST : SOF_COMP_DAI;

	ret = pipe_ipc_do(sp, fileread, fileread->comp.hdr.size);
	if (ret < 0) {
		SNDERR("error: IPC failed %d\n", ret);
	}

	free(fileread);
	return ret;
}

/* load filewrite component */
static int load_filewrite(struct sof_pipe *sp, struct tplg_context *ctx, int dir)
{
	struct sof_ipc_comp_file *filewrite;
	int ret;

	filewrite = calloc(1, sizeof(struct sof_ipc_comp_file)  + UUID_SIZE);
	if (!filewrite)
		return -ENOMEM;

	ret = plug_load_filewrite(ctx, filewrite);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx, ctx->widget->num_kcontrols, NULL, 0, NULL) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	filewrite->direction = dir;

	/* Set type depending on direction */
	filewrite->comp.type = (dir == SOF_IPC_STREAM_PLAYBACK) ?
		SOF_COMP_DAI : SOF_COMP_HOST;

	ret = pipe_ipc_do(sp, filewrite, filewrite->comp.hdr.size);
	if (ret < 0) {
		SNDERR("error: IPC failed %d\n", ret);
	}

	free(filewrite);
	return ret;
}

static int plug_aif_in_out(struct sof_pipe *sp, struct tplg_context *ctx, int dir)
{
	if (dir == SOF_IPC_STREAM_PLAYBACK)
		return load_fileread(sp, ctx, dir);
	else
		return load_filewrite(sp, ctx, dir);
}

static int plug_dai_in_out(struct sof_pipe *sp, struct tplg_context *ctx, int dir)
{
	if (dir == SOF_IPC_STREAM_PLAYBACK)
		return load_filewrite(sp, ctx, dir);
	else
		return load_fileread(sp, ctx, dir);
}

#if 0
static int plug_ctl_init(struct tplg_context *ctx, struct plug_ctl_container *ctls,
			 struct snd_soc_tplg_ctl_hdr *tplg_ctl)
{
	if (ctls->count >= MAX_CTLS) {
		SNDERR("error: ctoo many CTLs in topology\n");
		return -EINVAL;
	}

	/* ignore kcontrols without a name */
	if (strnlen(tplg_ctl->name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN - 1) == 0)
		return 0;

	ctls->tplg[ctls->count] = calloc(1, ctx->hdr->payload_size);
	if (!ctls->tplg[ctls->count])
		return -ENOMEM;

	memcpy(ctls->tplg[ctls->count], tplg_ctl, ctx->hdr->payload_size);
	ctls->count++;

	return 0;
}
#endif

static int plug_new_pga_ipc(struct sof_pipe *sp, struct tplg_context *ctx)
{
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp_volume *volume =
		(struct sof_ipc_comp_volume *)tplg_object;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	ret = tplg_new_pga(ctx, &volume->comp, MAX_TPLG_OBJECT_SIZE,
			tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create PGA\n");
		goto out;
	}
	ret = pipe_ipc_do(sp, volume, volume->comp.hdr.size);
	if (ret < 0) {
		SNDERR("error: IPC failed %d\n", ret);
		goto out;
	}

out:
	free(tplg_ctl);
	return ret;
}

static int plug_new_mixer_ipc(struct sof_pipe *sp, struct tplg_context *ctx)
{
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp_mixer *mixer =
		(struct sof_ipc_comp_mixer *)tplg_object;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	ret = tplg_new_mixer(ctx, &mixer->comp, MAX_TPLG_OBJECT_SIZE,
				tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create mixer\n");
		goto out;
	}

	ret = pipe_ipc_do(sp, mixer, mixer->comp.hdr.size);
	if (ret < 0) {
		SNDERR("error: IPC failed %d\n", ret);
		goto out;
	}

out:
	free(tplg_ctl);
	return ret;
}

static int plug_new_src_ipc(struct sof_pipe *sp, struct tplg_context *ctx)
{
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp_src *src =
		(struct sof_ipc_comp_src *)tplg_object;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	ret = tplg_new_src(ctx, &src->comp, MAX_TPLG_OBJECT_SIZE,
			tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create src\n");
		goto out;
	}
	ret = pipe_ipc_do(sp, src, src->comp.hdr.size);
	if (ret < 0) {
		SNDERR("error: IPC failed %d\n", ret);
		goto out;
	}

out:
	free(tplg_ctl);
	return ret;
}

static int plug_new_asrc_ipc(struct sof_pipe *sp, struct tplg_context *ctx)
{
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp_asrc *asrc =
		(struct sof_ipc_comp_asrc *)tplg_object;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	ret = tplg_new_asrc(ctx, &asrc->comp, MAX_TPLG_OBJECT_SIZE,
			tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create PGA\n");
		goto out;
	}
	ret = pipe_ipc_do(sp, asrc, asrc->comp.hdr.size);
	if (ret < 0) {
		SNDERR("error: IPC failed %d\n", ret);
		goto out;
	}

out:
	free(tplg_ctl);
	return ret;
}

static int plug_new_process_ipc(struct sof_pipe *sp, struct tplg_context *ctx)
{
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp_process *process =
		(struct sof_ipc_comp_process *)tplg_object;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	ret = tplg_new_process(ctx, &process->comp, MAX_TPLG_OBJECT_SIZE,
			tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create PGA\n");
		goto out;
	}
	ret = pipe_ipc_do(sp, process, process->comp.hdr.size);
	if (ret < 0) {
		SNDERR("error: IPC failed %d\n", ret);
		goto out;
	}

out:
	free(tplg_ctl);
	return ret;
}

static int plug_new_pipeline_ipc(struct sof_pipe *sp, struct tplg_context *ctx)
{
	struct sof_ipc_pipe_new pipeline = {0};
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	ret = tplg_new_pipeline(ctx, &pipeline, sizeof(pipeline), tplg_ctl);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create pipeline\n");
		goto out;
	}

	ret = pipe_ipc_do(sp, &pipeline, pipeline.hdr.size);
	if (ret < 0) {
		SNDERR("error: IPC failed %d\n", ret);
		goto out;
	}

out:
	free(tplg_ctl);
	return ret;
}

static int plug_new_buffer_ipc(struct sof_pipe *sp, struct tplg_context *ctx)
{
	struct sof_ipc_buffer buffer = {0};
	int ret;

	ret = tplg_new_buffer(ctx, &buffer, sizeof(buffer),
				NULL, 0);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create pipeline\n");
		return ret;
	}

	// TODO: override the buffer size for host - use a topology flag
	buffer.size = (buffer.size / 378) * 1024 * 48;
	ret = pipe_ipc_do(sp, &buffer, buffer.comp.hdr.size);
	if (ret < 0) {
		SNDERR("error: IPC failed %d\n", ret);
	}

	return ret;
}

/*
 * create a list with all widget info
 * containing mapping between component names and ids
 * which will be used for setting up component connections
 */
static inline int insert_comp(struct sof_pipe *sp, struct tplg_context *ctx)
{
	struct tplg_comp_info *temp_comp_list = sp->comp_list;
	int comp_index = sp->info_index;
	int comp_id = ctx->comp_id;

	/* mapping should be empty */
	if (temp_comp_list[comp_index].name) {
		fprintf(stderr, "comp index %d already in use with %d:%s cant insert %d:%s\n",
			comp_index,
			temp_comp_list[comp_index].id, temp_comp_list[comp_index].name,
			ctx->widget->id, ctx->widget->name);
		return -EINVAL;
	}

	temp_comp_list[comp_index].id = comp_id;
	temp_comp_list[comp_index].name = ctx->widget->name;
	temp_comp_list[comp_index].type = ctx->widget->id;
	temp_comp_list[comp_index].pipeline_id = ctx->pipeline_id;

	printf("debug: loading comp_id %d: widget %s type %d size %d at offset %ld\n",
		comp_id, ctx->widget->name, ctx->widget->id, ctx->widget->size,
		ctx->tplg_offset);

	return 0;
}

/* load dapm widget */
int plug_load_widget(struct sof_pipe *sp, struct tplg_context *ctx)
{
	int ret = 0;

	/* get next widget */
	ctx->widget = tplg_get_widget(ctx);
	ctx->widget_size = ctx->widget->size;

	/* insert widget into mapping */
	ret = insert_comp(sp, ctx);
	if (ret < 0) {
		fprintf(stderr, "plug_load_widget: invalid widget index\n");
		return ret;
	}

	/* load widget based on type */
	switch (ctx->widget->id) {

	/* load pga widget */
	case SND_SOC_TPLG_DAPM_PGA:
		if (plug_new_pga_ipc(sp, ctx) < 0) {
			fprintf(stderr, "error: load pga\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_AIF_IN:
		if (plug_aif_in_out(sp, ctx, SOF_IPC_STREAM_PLAYBACK) < 0) {
			fprintf(stderr, "error: load AIF IN failed\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_AIF_OUT:
		if (plug_aif_in_out(sp, ctx, SOF_IPC_STREAM_CAPTURE) < 0) {
			fprintf(stderr, "error: load AIF OUT failed\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_DAI_IN:
		if (plug_dai_in_out(sp, ctx, SOF_IPC_STREAM_PLAYBACK) < 0) {
			fprintf(stderr, "error: load filewrite\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_DAI_OUT:
		if (plug_dai_in_out(sp, ctx, SOF_IPC_STREAM_CAPTURE) < 0) {
			fprintf(stderr, "error: load filewrite\n");
			ret = -EINVAL;
			goto exit;
		}
		break;

	case SND_SOC_TPLG_DAPM_BUFFER:
		if (plug_new_buffer_ipc(sp, ctx) < 0) {
			fprintf(stderr, "error: load pipeline\n");
			ret = -EINVAL;
			goto exit;
		}
		break;

	case SND_SOC_TPLG_DAPM_SCHEDULER:
		if (plug_new_pipeline_ipc(sp, ctx) < 0) {
			fprintf(stderr, "error: load pipeline\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_SRC:
		if (plug_new_src_ipc(sp, ctx) < 0) {
			fprintf(stderr, "error: load src\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_ASRC:
		if (plug_new_asrc_ipc(sp, ctx) < 0) {
			fprintf(stderr, "error: load asrc\n");
			ret = -EINVAL;
			goto exit;
		}
		break;

	case SND_SOC_TPLG_DAPM_MIXER:
		if (plug_new_mixer_ipc(sp, ctx) < 0) {
			fprintf(stderr, "error: load mixer\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_EFFECT:
		if (plug_new_process_ipc(sp, ctx) < 0) {
			fprintf(stderr, "error: load effect\n");
			ret = -EINVAL;
			goto exit;
		}
		break;

	/* unsupported widgets */
	default:
		printf("info: Widget %s id %d unsupported and skipped: size %d priv size %d\n",
			ctx->widget->name, ctx->widget->id,
			ctx->widget->size, ctx->widget->priv.size);

#if 0
		if (fseek(ctx->file, ctx->widget->priv.size, SEEK_CUR)) {
					fprintf(stderr, "error: fseek unsupported widget\n");
					ret = -errno;
					goto exit;
				}
		ret = tplg_create_controls(ctx->widget->num_kcontrols, ctx->file, NULL, 0);
		if (ret < 0) {
			fprintf(stderr, "error: loading controls\n");
			//goto exit;
		}
#endif
		ret = 0;
		break;
	}

	ret = 1;

exit:
	return ret;
}

int plug_register_graph(struct sof_pipe *sp, struct tplg_context *ctx,
			struct tplg_comp_info *temp_comp_list,
			char *pipeline_string,
			int count, int num_comps, int pipeline_id)
{
	struct sof_ipc_pipe_comp_connect connection = {0};
	struct sof_ipc_pipe_ready ready = {0};
	struct sof_ipc_comp_reply reply = {0};
	int ret = 0;
	int i;

	for (i = 0; i < count; i++) {

		ret = tplg_create_graph(ctx, num_comps, pipeline_id,
					temp_comp_list, pipeline_string,
					&connection, i);
		if (ret < 0)
			return ret;

		ret = pipe_ipc_do(sp, &connection, connection.hdr.size);
		if (ret < 0) {
			SNDERR("error: IPC failed %d\n", ret);
			return ret;
		}
	}

	/* pipeline complete after pipeline connections are established */
	for (i = 0; i < num_comps; i++) {

		if (temp_comp_list[i].pipeline_id == pipeline_id &&
		    temp_comp_list[i].type == SND_SOC_TPLG_DAPM_SCHEDULER) {

			ready.comp_id = temp_comp_list[i].id;
			ready.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_PIPE_COMPLETE;
			ready.hdr.size = sizeof(reply);

			ret = pipe_ipc_do(sp, &ready, ready.hdr.size);
			if (ret < 0) {
				SNDERR("error: IPC failed %d\n", ret);
				return ret;
			}
		}
	}

	return ret;
}

/* parse topology file and set up pipeline */
int plug_parse_topology(struct sof_pipe *sp, struct tplg_context *ctx)

{
	struct snd_soc_tplg_hdr *hdr;
	struct tplg_comp_info *comp_list_realloc = NULL;
	char pipeline_string[256] = {0};
	int i;
	int ret = 0;
	FILE *file;
	size_t size;

	/* ctl callback */
	ctx->ctl_arg = sp;
	ctx->ctl_cb = pipe_kcontrol_cb_new;

	/* open topology file */
	file = fopen(ctx->tplg_file, "rb");
	if (!file) {
		fprintf(stderr, "error: can't open topology %s : %s\n", ctx->tplg_file,
			strerror(errno));
		return -errno;
	}

	/* file size */
	if (fseek(file, 0, SEEK_END)) {
		fprintf(stderr, "error: can't seek to end of topology: %s\n",
			strerror(errno));
		fclose(file);
		return -errno;
	}
	ctx->tplg_size = ftell(file);
	if (fseek(file, 0, SEEK_SET)) {
		fprintf(stderr, "error: can't seek to beginning of topology: %s\n",
			strerror(errno));
		fclose(file);
		return -errno;
	}

	/* load whole topology into memory */
	ctx->tplg_base = calloc(ctx->tplg_size, 1);
	if (!ctx->tplg_base) {
		fprintf(stderr, "error: can't alloc buffer for topology %zu bytes\n",
			ctx->tplg_size);
		fclose(file);
		return -ENOMEM;
	}
	ret = fread(ctx->tplg_base, ctx->tplg_size, 1, file);
	if (ret != 1) {
		fprintf(stderr, "error: can't read topology: %s\n",
			strerror(errno));
		fclose(file);
		return -errno;
	}
	fclose(file);

	while (ctx->tplg_offset < ctx->tplg_size) {

		/* read next topology header */
		hdr = tplg_get_hdr(ctx);

		fprintf(stdout, "type: %x, size: 0x%x count: %d index: %d\n",
			hdr->type, hdr->payload_size, hdr->count, hdr->index);

		ctx->hdr = hdr;

		/* parse header and load the next block based on type */
		switch (hdr->type) {
		/* load dapm widget */
		case SND_SOC_TPLG_TYPE_DAPM_WIDGET:

			fprintf(stdout, "number of DAPM widgets %d\n",
				hdr->count);

			/* update max pipeline_id */
			ctx->pipeline_id = hdr->index;

			sp->info_elems += hdr->count;
			size = sizeof(struct tplg_comp_info) * sp->info_elems;
			comp_list_realloc = (struct tplg_comp_info *)
					 realloc(sp->comp_list, size);

			if (!comp_list_realloc && size) {
				fprintf(stderr, "error: mem realloc\n");
				ret = -errno;
				goto out;
			}
			sp->comp_list = comp_list_realloc;

			for (i = (sp->info_elems - hdr->count); i < sp->info_elems; i++)
				sp->comp_list[i].name = NULL;

			for (sp->info_index = (sp->info_elems - hdr->count);
			     sp->info_index < sp->info_elems;
			     sp->info_index++) {
				ret = plug_load_widget(sp, ctx);
				if (ret < 0) {
					printf("error: loading widget\n");
					goto finish;
				} else if (ret > 0)
					ctx->comp_id++;
			}
			break;

		/* set up component connections from pipeline graph */
		case SND_SOC_TPLG_TYPE_DAPM_GRAPH:
			if (plug_register_graph(sp, ctx, sp->comp_list,
						pipeline_string,
						hdr->count,
						ctx->comp_id,
						hdr->index) < 0) {
				fprintf(stderr, "error: pipeline graph\n");
				ret = -EINVAL;
				goto out;
			}
			break;

		default:
			printf("%s %d\n", __func__, __LINE__);
			tplg_skip_hdr_payload(ctx);
			break;
		}
	}
finish:

out:
	/* free all data */
	free(sp->comp_list);
	return ret;
}
