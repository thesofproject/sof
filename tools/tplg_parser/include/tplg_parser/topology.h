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

#define SOF_DEV 1
#define FUZZER_DEV 2

#define MOVE_POINTER_BY_BYTES(p, b) ((typeof(p))((uint8_t *)(p) + (b)))

struct testbench_prm;
struct snd_soc_tplg_vendor_array;
struct snd_soc_tplg_ctl_hdr;
struct sof_topology_token;

struct comp_info {
	char *name;
	int id;
	int type;
	int pipeline_id;
};

struct frame_types {
	char *name;
	enum sof_ipc_frame frame;
};

struct sof;
struct fuzz;

/*
 * Per topology data.
 *
 * TODO: Some refactoring still required to move pipeline specific data.
 */
struct tplg_context {
	/* info array */
	struct comp_info *info;		/* comp info array */
	int info_elems;
	int info_index;

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

	/*
	 * input and output sample rate parameters
	 * By default, these are calculated from pipeline frames_per_sched
	 * and period but they can also be overridden via input arguments
	 * to the testbench.
	 */
	uint32_t fs_in;
	uint32_t fs_out;
	uint32_t channels_in;
	uint32_t channels_out;
	enum sof_ipc_frame frame_fmt;

	/* global data */
	FILE *file;
	struct testbench_prm *tp;
	struct sof *sof;
	const char *tplg_file;
	struct fuzz *fuzzer;
};

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
};

enum sof_ipc_frame find_format(const char *name);

int get_token_uint32_t(void *elem, void *object, uint32_t offset,
		       uint32_t size);

int get_token_comp_format(void *elem, void *object, uint32_t offset,
			  uint32_t size);

int get_token_uuid(void *elem, void *object, uint32_t offset, uint32_t size);

/* EFFECT */
int get_token_process_type(void *elem, void *object, uint32_t offset,
			   uint32_t size);

/* DAI */
enum sof_ipc_dai_type find_dai(const char *name);

int get_token_dai_type(void *elem, void *object, uint32_t offset,
		       uint32_t size);

struct sof_dai_types {
	const char *name;
	enum sof_ipc_dai_type type;
};

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
int get_token_dai_type(void *elem, void *object, uint32_t offset,
		       uint32_t size);
enum sof_ipc_dai_type find_dai(const char *name);

enum sof_ipc_process_type tplg_get_process_name(const char *name);
enum sof_comp_type tplg_get_process_type(enum sof_ipc_process_type type);

int tplg_process_append_data(struct sof_ipc_comp_process **process_ipc,
			       struct sof_ipc_comp_process *process,
			       struct snd_soc_tplg_ctl_hdr *ctl,
			       char *priv_data);

int tplg_process_init_data(struct sof_ipc_comp_process **process_ipc,
			     struct sof_ipc_comp_process *process);

int tplg_read_array(struct snd_soc_tplg_vendor_array *array, FILE *file);
int tplg_create_buffer(struct tplg_context *ctx,
		     struct sof_ipc_buffer *buffer);
int tplg_new_buffer(struct tplg_context *ctx, struct sof_ipc_buffer *buffer,
		struct snd_soc_tplg_ctl_hdr *rctl);

int tplg_create_pcm(struct tplg_context *ctx, int dir,
		  struct sof_ipc_comp_host *host);
int tplg_create_dai(struct tplg_context *ctx,
		  struct sof_ipc_comp_dai *comp_dai);
int tplg_create_pga(struct tplg_context *ctx, struct sof_ipc_comp_volume *volume, size_t max_comp_size);
int tplg_create_pipeline(struct tplg_context *ctx,
		       struct sof_ipc_pipe_new *pipeline);
int tplg_new_pipeline(struct tplg_context *ctx, struct sof_ipc_pipe_new *pipeline,
		struct snd_soc_tplg_ctl_hdr *rctl);

int tplg_create_single_control(struct snd_soc_tplg_ctl_hdr **ctl, char **priv,
			  FILE *file);
int tplg_create_controls(int num_kcontrols, FILE *file, struct snd_soc_tplg_ctl_hdr *rctl,
		size_t max_ctl_size);

int tplg_create_src(struct tplg_context *ctx,
		  struct sof_ipc_comp_src *src, size_t max_comp_size);
int tplg_new_src(struct tplg_context *ctx, struct sof_ipc_comp *comp, size_t comp_size,
		struct snd_soc_tplg_ctl_hdr *rctl, size_t ctl_size);

int tplg_create_asrc(struct tplg_context *ctx,
		   struct sof_ipc_comp_asrc *asrc, size_t max_comp_size);
int tplg_new_asrc(struct tplg_context *ctx, struct sof_ipc_comp *comp, size_t comp_size,
		struct snd_soc_tplg_ctl_hdr *rctl, size_t ctl_size);

int tplg_create_mixer(struct tplg_context *ctx,
		    struct sof_ipc_comp_mixer *mixer, size_t max_comp_size);
int tplg_create_process(struct tplg_context *ctx,
		      struct sof_ipc_comp_process *process,
		      struct sof_ipc_comp_ext *comp_ext);
int tplg_new_process(struct tplg_context *ctx, struct sof_ipc_comp *comp, size_t comp_size,
		struct snd_soc_tplg_ctl_hdr *rctl, size_t ctl_size);

int tplg_create_graph(int num_comps, int pipeline_id,
		    struct comp_info *temp_comp_list, char *pipeline_string,
		    struct sof_ipc_pipe_comp_connect *connection, FILE *file,
		    int route_num, int count);

int tplg_new_pga(struct tplg_context *ctx, struct sof_ipc_comp *comp, size_t comp_size,
		struct snd_soc_tplg_ctl_hdr *rctl, size_t ctl_size);
int tplg_register_pga(struct tplg_context *ctx);
int tplg_register_pga_ipc(struct tplg_context *ctx, struct snd_soc_tplg_ctl_hdr *ctl,
			  char *mailbox, size_t size);

int load_aif_in_out(struct tplg_context *ctx, int dir);
int load_dai_in_out(struct tplg_context *ctx, int dir);
int tplg_register_buffer(struct tplg_context *ctx);
int tplg_register_pipeline(struct tplg_context *ctx);
int tplg_register_src(struct tplg_context *ctx);
int tplg_register_asrc(struct tplg_context *ctx);

int tplg_register_mixer(struct tplg_context *ctx);
int tplg_new_mixer(struct tplg_context *ctx, struct sof_ipc_comp *comp, size_t comp_size,
		struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size);

int tplg_register_graph(void *dev, struct comp_info *temp_comp_list,
			char *pipeline_string, FILE *file,
			int count, int num_comps, int pipeline_id);
int load_process(struct tplg_context *ctx);
int load_widget(struct tplg_context *ctx);

void register_comp(int comp_type, struct sof_ipc_comp_ext *comp_ext);
int find_widget(struct comp_info *temp_comp_list, int count, char *name);
bool is_valid_priv_size(size_t size_read, size_t priv_size,
			struct snd_soc_tplg_vendor_array *array);

int parse_topology(struct tplg_context *ctx);

#endif
