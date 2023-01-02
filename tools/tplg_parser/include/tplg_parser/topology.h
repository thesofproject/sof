/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

#ifndef _COMMON_TPLG_H
#define _COMMON_TPLG_H

#include <stdbool.h>
#include <stddef.h>
#include <ipc/dai.h>
#include <ipc/topology.h>
#include <ipc/stream.h>
#include <kernel/tokens.h>

/* temporary - current MAXLEN is not define in UAPI header - fix pending */
#ifndef SNDRV_CTL_ELEM_ID_NAME_MAXLEN
#define SNDRV_CTL_ELEM_ID_NAME_MAXLEN	44
#endif

#include <alsa/sound/asoc.h>

#define TPLG_PARSER_SOF_DEV 1
#define TPLG_PARSER_FUZZER_DEV 2

#define MOVE_POINTER_BY_BYTES(p, b) ((typeof(p))((uint8_t *)(p) + (b)))

struct testbench_prm;
struct snd_soc_tplg_vendor_array;
struct snd_soc_tplg_ctl_hdr;
struct sof_topology_token;
struct sof;
struct fuzz;
struct sof_topology_module_desc;

struct tplg_comp_info {
	char *name;
	int id;
	int type;
	int pipeline_id;
};

/*
 * Per topology data.
 *
 * TODO: Some refactoring still required to move pipeline specific data.
 */
struct tplg_context {

	/* pipeline and core IDs we are processing */
	int pipeline_id;
	int core_id;

	/* current IPC object and widget */
	struct snd_soc_tplg_hdr *hdr;
	struct snd_soc_tplg_dapm_widget *widget;
	int comp_id;
	size_t widget_size;
	int dev_type;
	int sched_id;
	int dir;

	/* global data */
	void *tplg_base;
	size_t tplg_size;
	long tplg_offset;
	struct sof *sof;
	const char *tplg_file;
	struct fuzz *fuzzer;
	int ipc_major;

	/* kcontrol creation */
	void *ctl_arg;
	int (*ctl_cb)(struct snd_soc_tplg_ctl_hdr *tplg_ctl,
		      void *comp, void *arg);
};

#define tplg_get(ctx) ((void *)(ctx->tplg_base + ctx->tplg_offset))

#define tplg_get_hdr(ctx)							\
	({struct snd_soc_tplg_hdr *ptr;						\
	ptr = (struct snd_soc_tplg_hdr *)(ctx->tplg_base + ctx->tplg_offset);	\
	if (ptr->size != sizeof(*ptr)) {					\
		printf("%s %d hdr size mismatch 0x%x:0x%lx at offset %ld\n",	\
				__func__, __LINE__, ptr->size, sizeof(*ptr),	\
				ctx->tplg_offset); assert(0);			\
	}									\
	ctx->tplg_offset += sizeof(*ptr); (void *)ptr; })

#define tplg_skip_hdr_payload(ctx)						\
	({struct snd_soc_tplg_hdr *ptr;						\
	ptr = (struct snd_soc_tplg_hdr *)(ctx->tplg_base + ctx->tplg_offset);	\
	ctx->tplg_offset += hdr->payload_size; (void *)ptr; })

#define tplg_get_object(ctx, obj)						\
	({void *ptr; ptr = ctx->tplg_base + ctx->tplg_offset;			\
	ctx->tplg_offset += sizeof(*(obj)); ptr; })

#define tplg_get_object_priv(ctx, obj, priv_size)				\
	({void *ptr; ptr = ctx->tplg_base + ctx->tplg_offset;			\
	ctx->tplg_offset += sizeof(*(obj)) + priv_size; ptr; })

#define tplg_get_widget(ctx)							\
	({struct snd_soc_tplg_dapm_widget *w;					\
	w = (struct snd_soc_tplg_dapm_widget *)(ctx->tplg_base + ctx->tplg_offset); \
	ctx->tplg_offset += sizeof(*w) + w->priv.size; w; })

#define tplg_get_graph(ctx)							\
	({struct snd_soc_tplg_dapm_graph_elem *w;				\
	w = (struct snd_soc_tplg_dapm_graph_elem *)(ctx->tplg_base + ctx->tplg_offset); \
	ctx->tplg_offset += sizeof(*w); w; })

static inline int tplg_valid_widget(struct snd_soc_tplg_dapm_widget *widget)
{
	if (widget->size == sizeof(struct snd_soc_tplg_dapm_widget))
		return 1;
	else
		return 0;
}

enum sof_ipc_frame tplg_find_format(const char *name);

int tplg_token_get_uint32_t(void *elem, void *object, uint32_t offset,
			    uint32_t size);

int tplg_token_get_comp_format(void *elem, void *object, uint32_t offset,
			       uint32_t size);

int tplg_token_get_uuid(void *elem, void *object, uint32_t offset, uint32_t size);

int sof_parse_tokens(void *object,
		     const struct sof_topology_token *tokens,
		     int count, struct snd_soc_tplg_vendor_array *array,
		     int priv_size);

int sof_parse_string_tokens(void *object,
			    const struct sof_topology_token *tokens,
			    int count,
			    struct snd_soc_tplg_vendor_array *array);

int sof_parse_uuid_tokens(void *object,
			  const struct sof_topology_token *tokens,
			  int count,
			  struct snd_soc_tplg_vendor_array *array);

int sof_parse_word_tokens(void *object,
			  const struct sof_topology_token *tokens,
			  int count,
			  struct snd_soc_tplg_vendor_array *array);

int tplg_read_array(struct snd_soc_tplg_vendor_array *array);

int tplg_new_buffer(struct tplg_context *ctx, void *buffer, size_t buffer_size,
		    struct snd_soc_tplg_ctl_hdr *rctl, size_t buffer_ctl_size);

int tplg_create_dai(struct tplg_context *ctx,
		    struct sof_ipc_comp_dai *comp_dai);

int tplg_create_pipeline(struct tplg_context *ctx,
			 struct sof_ipc_pipe_new *pipeline);

int tplg_get_single_control(struct tplg_context *ctx,
			    struct snd_soc_tplg_ctl_hdr **ctl,
			    struct snd_soc_tplg_private **priv);

int tplg_create_controls(struct tplg_context *ctx, int num_kcontrols,
			 struct snd_soc_tplg_ctl_hdr *rctl,
			 size_t max_ctl_size, void *object);

int tplg_new_pcm(struct tplg_context *ctx, void *host, size_t host_size);

int tplg_new_pipeline(struct tplg_context *ctx, void *pipeline,
		      size_t pipeline_size, struct snd_soc_tplg_ctl_hdr *rctl);

int tplg_new_mixer(struct tplg_context *ctx, void *mixer, size_t mixer_size,
		   struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size);

int tplg_new_src(struct tplg_context *ctx, void *src, size_t src_size,
		 struct snd_soc_tplg_ctl_hdr *rctl, size_t ctl_size);

int tplg_new_asrc(struct tplg_context *ctx, void *asrc, size_t asrc_size,
		  struct snd_soc_tplg_ctl_hdr *rctl, size_t ctl_size);

int tplg_new_pga(struct tplg_context *ctx, void *pga, size_t pga_size,
		 struct snd_soc_tplg_ctl_hdr *rctl, size_t ctl_size);

int tplg_new_dai(struct tplg_context *ctx, void *dai, size_t dai_size,
		 struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size);

int tplg_new_process(struct tplg_context *ctx, void *process, size_t process_size,
		     struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size);

int tplg_create_graph(struct tplg_context *ctx, int count, int pipeline_id,
		      struct tplg_comp_info *temp_comp_list, char *pipeline_string,
		      struct sof_ipc_pipe_comp_connect *connection,
		      int route_num);

bool tplg_is_valid_priv_size(size_t size_read, size_t priv_size,
			     struct snd_soc_tplg_vendor_array *array);

int tplg_create_object(struct tplg_context *ctx,
		       const struct sof_topology_module_desc *desc, int num_desc,
		       const char *name, void *object, size_t max_object_size);

#endif
