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
#include <sound/asoc.h>
#include <ipc/dai.h>
#include <kernel/tokens.h>

#define SOF_DEV 1
#define FUZZER_DEV 2

struct testbench_prm;

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

	/* global data */
	FILE *file;
	struct testbench_prm *tp;
	struct sof *sof;
	const char *tplg_file;
	struct fuzz *fuzzer;
};

static const struct frame_types sof_frames[] = {
	/* TODO: fix topology to use ALSA formats */
	{"s16le", SOF_IPC_FRAME_S16_LE},
	{"s24le", SOF_IPC_FRAME_S24_4LE},
	{"s32le", SOF_IPC_FRAME_S32_LE},
	{"float", SOF_IPC_FRAME_FLOAT},
	/* ALSA formats */
	{"S16_LE", SOF_IPC_FRAME_S16_LE},
	{"S24_LE", SOF_IPC_FRAME_S24_4LE},
	{"S32_LE", SOF_IPC_FRAME_S32_LE},
	{"FLOAT_LE", SOF_IPC_FRAME_FLOAT},
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

struct sof_topology_token {
	uint32_t token;
	uint32_t type;
	int (*get_token)(void *elem, void *object, uint32_t offset,
			 uint32_t size);
	uint32_t offset;
	uint32_t size;
};

enum sof_ipc_frame find_format(const char *name);

int get_token_uint32_t(void *elem, void *object, uint32_t offset,
		       uint32_t size);

int get_token_comp_format(void *elem, void *object, uint32_t offset,
			  uint32_t size);

int get_token_uuid(void *elem, void *object, uint32_t offset, uint32_t size);

/* Buffers */
static const struct sof_topology_token buffer_tokens[] = {
	{SOF_TKN_BUF_SIZE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_buffer, size), 0},
	{SOF_TKN_BUF_CAPS, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_buffer, caps), 0},
};

static const struct sof_topology_token buffer_comp_tokens[] = {
	{SOF_TKN_COMP_CORE_ID, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp, core), 0},
};

/* scheduling */
static const struct sof_topology_token sched_tokens[] = {
	{SOF_TKN_SCHED_PERIOD, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_pipe_new, period), 0},
	{SOF_TKN_SCHED_PRIORITY, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_pipe_new, priority), 0},
	{SOF_TKN_SCHED_MIPS, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_pipe_new, period_mips), 0},
	{SOF_TKN_SCHED_CORE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_pipe_new, core), 0},
	{SOF_TKN_SCHED_FRAMES, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_pipe_new, frames_per_sched), 0},
	{SOF_TKN_SCHED_TIME_DOMAIN, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_pipe_new, time_domain), 0},
};

/* volume */
static const struct sof_topology_token volume_tokens[] = {
	{SOF_TKN_VOLUME_RAMP_STEP_TYPE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_volume, ramp), 0},
	{SOF_TKN_VOLUME_RAMP_STEP_MS,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp_volume, initial_ramp), 0},
};

/* SRC */
static const struct sof_topology_token src_tokens[] = {
	{SOF_TKN_SRC_RATE_IN, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_src, source_rate), 0},
	{SOF_TKN_SRC_RATE_OUT, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_src, sink_rate), 0},
};

/* ASRC */
static const struct sof_topology_token asrc_tokens[] = {
	{SOF_TKN_ASRC_RATE_IN, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, source_rate), 0},
	{SOF_TKN_ASRC_RATE_OUT, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, sink_rate), 0},
	{SOF_TKN_ASRC_ASYNCHRONOUS_MODE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, asynchronous_mode), 0},
	{SOF_TKN_ASRC_OPERATION_MODE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, operation_mode), 0},
};

/* EFFECT */
int get_token_process_type(void *elem, void *object, uint32_t offset,
			   uint32_t size);

static const struct sof_topology_token process_tokens[] = {
	{SOF_TKN_PROCESS_TYPE, SND_SOC_TPLG_TUPLE_TYPE_STRING,
		get_token_process_type,
		offsetof(struct sof_ipc_comp_process, type), 0},
};

/* Tone */
static const struct sof_topology_token tone_tokens[] = {
};

/* Generic components */
static const struct sof_topology_token comp_tokens[] = {
	{SOF_TKN_COMP_PERIOD_SINK_COUNT,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp_config, periods_sink), 0},
	{SOF_TKN_COMP_PERIOD_SOURCE_COUNT,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp_config, periods_source), 0},
	{SOF_TKN_COMP_FORMAT,
		SND_SOC_TPLG_TUPLE_TYPE_STRING, get_token_comp_format,
		offsetof(struct sof_ipc_comp_config, frame_fmt), 0},
};

/* PCM */
static const struct sof_topology_token pcm_tokens[] = {
	{SOF_TKN_PCM_DMAC_CONFIG, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_host, dmac_config), 0},
};

/* DAI */
enum sof_ipc_dai_type find_dai(const char *name);

int get_token_dai_type(void *elem, void *object, uint32_t offset,
		       uint32_t size);
static const struct sof_topology_token dai_tokens[] = {
	{SOF_TKN_DAI_TYPE, SND_SOC_TPLG_TUPLE_TYPE_STRING, get_token_dai_type,
		offsetof(struct sof_ipc_comp_dai, type), 0},
	{SOF_TKN_DAI_INDEX, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp_dai, dai_index), 0},
	{SOF_TKN_DAI_DIRECTION, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	get_token_uint32_t,
	offsetof(struct sof_ipc_comp_dai, direction), 0},
};

/* Component extended tokens */
static const struct sof_topology_token comp_ext_tokens[] = {
	{SOF_TKN_COMP_UUID,
		SND_SOC_TPLG_TUPLE_TYPE_UUID, get_token_uuid,
		offsetof(struct sof_ipc_comp_ext, uuid), 0},
};

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

int tplg_read_array(struct snd_soc_tplg_vendor_array *array, FILE *file);
int tplg_load_buffer(struct tplg_context *ctx,
		     struct sof_ipc_buffer *buffer);
int tplg_load_pcm(struct tplg_context *ctx, int dir,
		  struct sof_ipc_comp_host *host);
int tplg_load_dai(struct tplg_context *ctx,
		  struct sof_ipc_comp_dai *comp_dai);
int tplg_load_pga(struct tplg_context *ctx,
		  struct sof_ipc_comp_volume *volume);
int tplg_load_pipeline(struct tplg_context *ctx,
		       struct sof_ipc_pipe_new *pipeline);
int tplg_load_one_control(struct snd_soc_tplg_ctl_hdr **ctl, char **priv,
			  FILE *file);
int tplg_load_controls(int num_kcontrols, FILE *file);
int tplg_load_src(struct tplg_context *ctx,
		  struct sof_ipc_comp_src *src);
int tplg_load_asrc(struct tplg_context *ctx,
		   struct sof_ipc_comp_asrc *asrc);
int tplg_load_mixer(struct tplg_context *ctx,
		    struct sof_ipc_comp_mixer *mixer);
int tplg_load_process(struct tplg_context *ctx,
		      struct sof_ipc_comp_process *process,
		      struct sof_ipc_comp_ext *comp_ext);
int tplg_load_graph(int num_comps, int pipeline_id,
		    struct comp_info *temp_comp_list, char *pipeline_string,
		    struct sof_ipc_pipe_comp_connect *connection, FILE *file,
		    int route_num, int count);

int load_pga(struct tplg_context *ctx);

int load_aif_in_out(struct tplg_context *ctx, int dir);
int load_dai_in_out(struct tplg_context *ctx, int dir);
int load_buffer(struct tplg_context *ctx);
int load_pipeline(struct tplg_context *ctx);
int load_src(struct tplg_context *ctx);
int load_asrc(struct tplg_context *ctx);
int load_mixer(struct tplg_context *ctx);
int load_process(struct tplg_context *ctx);
int load_widget(struct tplg_context *ctx);

void register_comp(int comp_type, struct sof_ipc_comp_ext *comp_ext);
int find_widget(struct comp_info *temp_comp_list, int count, char *name);
bool is_valid_priv_size(size_t size_read, size_t priv_size,
			struct snd_soc_tplg_vendor_array *array);

int parse_topology(struct tplg_context *ctx);

#endif
