// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

/* Topology parser */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <ipc/topology.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/topology.h>
#include <kernel/header.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

/** \brief Types of processing components */
enum sof_ipc_process_type {
	SOF_PROCESS_NONE = 0,		/**< None */
	SOF_PROCESS_EQFIR,		/**< Intel FIR */
	SOF_PROCESS_EQIIR,		/**< Intel IIR */
	SOF_PROCESS_KEYWORD_DETECT,	/**< Keyword Detection */
	SOF_PROCESS_KPB,		/**< KeyPhrase Buffer Manager */
	SOF_PROCESS_CHAN_SELECTOR,	/**< Channel Selector */
	SOF_PROCESS_MUX,
	SOF_PROCESS_DEMUX,
	SOF_PROCESS_DCBLOCK,
	SOF_PROCESS_DRC,
	SOF_PROCESS_MULTIBAND_DRC,
	SOF_PROCESS_TDFB,
};

struct sof_process_types {
	const char *name;
	enum sof_ipc_process_type type;
	enum sof_comp_type comp_type;
};

static const struct sof_process_types ipc3_process[] = {
	{"EQFIR", SOF_PROCESS_EQFIR, SOF_COMP_MODULE_ADAPTER},
	{"EQIIR", SOF_PROCESS_EQIIR, SOF_COMP_MODULE_ADAPTER},
	{"KEYWORD_DETECT", SOF_PROCESS_KEYWORD_DETECT, SOF_COMP_KEYWORD_DETECT},
	{"KPB", SOF_PROCESS_KPB, SOF_COMP_KPB},
	{"CHAN_SELECTOR", SOF_PROCESS_CHAN_SELECTOR, SOF_COMP_MODULE_ADAPTER},
	{"MUX", SOF_PROCESS_MUX, SOF_COMP_MODULE_ADAPTER},
	{"DEMUX", SOF_PROCESS_DEMUX, SOF_COMP_MODULE_ADAPTER},
	{"DCBLOCK", SOF_PROCESS_DCBLOCK, SOF_COMP_MODULE_ADAPTER},
	{"DRC", SOF_PROCESS_DRC, SOF_COMP_MODULE_ADAPTER},
	{"MULTIBAND_DRC", SOF_PROCESS_MULTIBAND_DRC, SOF_COMP_MODULE_ADAPTER},
	{"TDFB", SOF_PROCESS_TDFB, SOF_COMP_MODULE_ADAPTER},
};

static enum sof_ipc_process_type process_get_name(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ipc3_process); i++) {
		if (strcmp(name, ipc3_process[i].name) == 0)
			return ipc3_process[i].type;
	}

	return SOF_PROCESS_NONE;
}

static enum sof_comp_type process_get_type(enum sof_ipc_process_type type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ipc3_process); i++) {
		if (ipc3_process[i].type == type)
			return ipc3_process[i].comp_type;
	}

	return SOF_COMP_NONE;
}

static int process_token_get_type(void *elem, void *object, uint32_t offset,
				  uint32_t size)
{
	struct snd_soc_tplg_vendor_string_elem *velem = elem;
	uint32_t *val = (uint32_t *)((uint8_t *)object + offset);

	*val = process_get_name(velem->string);
	return 0;
}

static const struct sof_topology_token process_tokens[] = {
	{SOF_TKN_PROCESS_TYPE, SND_SOC_TPLG_TUPLE_TYPE_STRING,
		process_token_get_type,
		offsetof(struct sof_ipc_comp_process, type), 0},
};

/* process - IPC3 */
static const struct sof_topology_token_group process_ipc3_tokens[] = {
	{comp_tokens, ARRAY_SIZE(comp_tokens),
		offsetof(struct sof_ipc_comp_process, config)},
	{process_tokens, ARRAY_SIZE(process_tokens),
		0},
	{comp_ext_tokens, ARRAY_SIZE(comp_ext_tokens),
		sizeof(struct sof_ipc_comp_process)},
};

static int process_ipc3_build(struct tplg_context *ctx, void *_process)
{
	struct sof_ipc_comp_process *process = _process;
	int comp_id = ctx->comp_id;

	/* configure asrc */
	process->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	process->comp.id = comp_id;
	process->comp.hdr.size = sizeof(struct sof_ipc_comp_process) + UUID_SIZE;
	process->comp.type = process_get_type(process->type);
	process->comp.pipeline_id = ctx->pipeline_id;
	process->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	process->comp.ext_data_length = UUID_SIZE;

	return 0;
}

/* process - IPC4 */
static const struct sof_topology_token process4_tokens[] = {
	/* TODO */
};

static const struct sof_topology_token_group process_ipc4_tokens[] = {
	{process4_tokens, ARRAY_SIZE(process4_tokens)},
};

static int process_ipc4_build(struct tplg_context *ctx, void *process)
{
	return tplg_parse_widget_audio_formats(ctx);
}

static const struct sof_topology_module_desc process_ipc[] = {
	{3, process_ipc3_tokens, ARRAY_SIZE(process_ipc3_tokens),
		process_ipc3_build, sizeof(struct sof_ipc_comp_process)},
	{4, process_ipc4_tokens, ARRAY_SIZE(process_ipc4_tokens),
		process_ipc4_build},
};

static int process_append_data3(void *_process_ipc,
				struct snd_soc_tplg_ctl_hdr *ctl,
				struct snd_soc_tplg_private *priv_data,
				size_t max_process_size)
{
	struct sof_ipc_comp_process *process_ipc = _process_ipc;
	struct snd_soc_tplg_bytes_control *bytes_ctl;
	size_t size, ipc_size;

	if (ctl->ops.info != SND_SOC_TPLG_CTL_BYTES)
		return 0;

	/* Size is process IPC plus private data minus ABI header */
	bytes_ctl = (struct snd_soc_tplg_bytes_control *)ctl;
	size = bytes_ctl->priv.size - sizeof(struct sof_abi_hdr);
	ipc_size = sizeof(struct sof_ipc_comp_process) + UUID_SIZE +
			process_ipc->size + size;

	/* validate if everything will fit */
	if (ipc_size > max_process_size) {
		fprintf(stderr, "error: process priv data too big, have %zu need %zu\n",
			max_process_size, ipc_size);
		return -EINVAL;
	}

	/* Copy configuration data, need to strip ABI header*/
	memcpy((char *)process_ipc + sizeof(struct sof_ipc_comp_process)
	       + UUID_SIZE + process_ipc->size,
	       (char *)priv_data->data + sizeof(struct sof_abi_hdr), size);
	process_ipc->size += size;
	return 0;
}

static int process_append_data4(void *_process_ipc,
				struct snd_soc_tplg_ctl_hdr *ctl,
				struct snd_soc_tplg_private *priv_data,
				size_t max_process_size)
{
	struct sof_ipc_comp_process *process_ipc = _process_ipc;
	struct snd_soc_tplg_bytes_control *bytes_ctl;
	size_t size;

	if (ctl->ops.info != SND_SOC_TPLG_CTL_BYTES)
		return 0;

	/* Size is process IPC plus private data minus ABI header */
	bytes_ctl = (struct snd_soc_tplg_bytes_control *)ctl;
	size = bytes_ctl->priv.size - sizeof(struct sof_abi_hdr);

	/* validate if everything will fit */
	if (size > max_process_size) {
		fprintf(stderr, "error: process priv data too big, have %zu need %zu\n",
			max_process_size, size);
		return -EINVAL;
	}

	/* Copy configuration data, need to strip ABI header */
	memcpy((char *)process_ipc + process_ipc->size,
	       (char *)priv_data->data + sizeof(struct sof_abi_hdr), size);
	process_ipc->size += size;

	fprintf(stdout, "process configuration data size %#x\n", process_ipc->size);

	return 0;
}

int tplg_new_process(struct tplg_context *ctx, void *process, size_t process_size,
		     struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size)
{
	struct snd_soc_tplg_dapm_widget *widget = ctx->widget;
	struct snd_soc_tplg_ctl_hdr *ctl = NULL;
	struct snd_soc_tplg_private *priv_data = NULL;
	int ret, i;

	ret = tplg_create_object(ctx, process_ipc, ARRAY_SIZE(process_ipc),
				 "process", process, process_size);

	/* Get control into ctl and priv_data */
	for (i = 0; i < widget->num_kcontrols; i++) {

		ret = tplg_get_single_control(ctx, &ctl, &priv_data);
		if (ret < 0) {
			fprintf(stderr, "error: failed control load\n");
			return ret;
		}

		/* call ctl creation callback if needed */
		if (ctx->ctl_cb)
			ctx->ctl_cb(ctl, process, ctx->ctl_arg, 0);

		/* Merge process and priv_data into process_ipc */
		if (!priv_data)
			continue;
		switch (ctx->ipc_major) {
		case 3:
			ret = process_append_data3(process, ctl,
						   priv_data, process_size);
			break;
		case 4:
			ret = process_append_data4(process, ctl, priv_data, process_size);
			break;
		default:
			break;
		}
		if (ret < 0) {
			fprintf(stderr, "error: failed to append process priv data\n");
			return ret;
		}
	}

	return ret;
}
