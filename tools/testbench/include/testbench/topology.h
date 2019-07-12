/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

#ifndef _COMMON_TPLG_H
#define _COMMON_TPLG_H

#include <sound/asoc.h>
#include "common_test.h"

/*
 * TODO: include these token from kernel uapi header
 * Tokens - must match values in topology configurations
 */

/* buffers */
#define SOF_TKN_BUF_SIZE                        100
#define SOF_TKN_BUF_CAPS                        101

/* DAI */
/* Token retired with ABI 3.2, do not use for new capabilities
 * #define      SOF_TKN_DAI_DMAC_CONFIG                 153
 */
#define SOF_TKN_DAI_TYPE                        154
#define SOF_TKN_DAI_INDEX                       155
#define SOF_TKN_DAI_DIRECTION                   156

/* scheduling */
#define SOF_TKN_SCHED_PERIOD                    200
#define SOF_TKN_SCHED_PRIORITY                  201
#define SOF_TKN_SCHED_MIPS                      202
#define SOF_TKN_SCHED_CORE                      203
#define SOF_TKN_SCHED_FRAMES                    204
#define SOF_TKN_SCHED_TIME_DOMAIN               205

/* volume */
#define SOF_TKN_VOLUME_RAMP_STEP_TYPE           250
#define SOF_TKN_VOLUME_RAMP_STEP_MS             251

/* SRC */
#define SOF_TKN_SRC_RATE_IN                     300
#define SOF_TKN_SRC_RATE_OUT                    301

/* PCM */
#define SOF_TKN_PCM_DMAC_CONFIG                 353

/* Generic components */
#define SOF_TKN_COMP_PERIOD_SINK_COUNT          400
#define SOF_TKN_COMP_PERIOD_SOURCE_COUNT        401
#define SOF_TKN_COMP_FORMAT                     402
/* Token retired with ABI 3.2, do not use for new capabilities
 * #define SOF_TKN_COMP_PRELOAD_COUNT              403
 */

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

struct sof_topology_token {
	uint32_t token;
	uint32_t type;
	int (*get_token)(void *elem, void *object, uint32_t offset,
			 uint32_t size);
	uint32_t offset;
	uint32_t size;
};

struct sof_dai_types {
	const char *name;
	enum sof_ipc_dai_type type;
};

enum sof_ipc_frame find_format(const char *name);

int get_token_uint32_t(void *elem, void *object, uint32_t offset,
		       uint32_t size);

int get_token_comp_format(void *elem, void *object, uint32_t offset,
			  uint32_t size);

enum sof_ipc_dai_type find_dai(const char *name);

int get_token_dai_type(void *elem, void *object, uint32_t offset,
		       uint32_t size);

/* Buffers */
static const struct sof_topology_token buffer_tokens[] = {
	{SOF_TKN_BUF_SIZE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_buffer, size), 0},
	{SOF_TKN_BUF_CAPS, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_buffer, caps), 0},
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
static const struct sof_topology_token dai_tokens[] = {
	{SOF_TKN_DAI_TYPE, SND_SOC_TPLG_TUPLE_TYPE_STRING, get_token_dai_type,
		offsetof(struct sof_ipc_comp_dai, type), 0},
	{SOF_TKN_DAI_INDEX, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp_dai, dai_index), 0},
	{SOF_TKN_DAI_DIRECTION, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	get_token_uint32_t,
	offsetof(struct sof_ipc_comp_dai, direction), 0},
};

int sof_parse_tokens(void *object,
		     const struct sof_topology_token *tokens,
		     int count, struct snd_soc_tplg_vendor_array *array,
		     int priv_size);
void sof_parse_string_tokens(void *object,
			     const struct sof_topology_token *tokens,
			     int count,
			     struct snd_soc_tplg_vendor_array *array);
void sof_parse_uuid_tokens(void *object,
			   const struct sof_topology_token *tokens,
			   int count,
			   struct snd_soc_tplg_vendor_array *array);
void sof_parse_word_tokens(void *object,
			   const struct sof_topology_token *tokens,
			   int count,
			   struct snd_soc_tplg_vendor_array *array);

int parse_topology(struct sof *sof, struct shared_lib_table *library_table,
		   struct testbench_prm *tp, int *fr_id, int *fw_id,
		   int *sched_id, char *pipeline_msg);

#endif
