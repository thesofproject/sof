/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 *	   Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *	   Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/*
 * Topology parser to parse topology bin file
 * and set up components and pipeline
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/sof.h>
#include <sof/ipc.h>
#include <sof/topology.h>

#if defined(CONFIG_TOPOLOGY)

#include <uapi/ipc/topology.h>
#include <uapi/user/tokens.h>
#include "topology-priv.h"

/* TODO: These traces are intentionally all converted to errors to be able to
 * see them in boot phase before the normal trace is up and running. Need to
 * fix this later.
 */
#define tracev_tplg(__e, ...) trace_error(TRACE_CLASS_PIPE, __e, ##__VA_ARGS__)
#define tracev_tplg_value(x)  trace_error_value(x)
#define tracev_tplg_error(__e, ...)				\
	trace_error(TRACE_CLASS_PIPE, __e, ##__VA_ARGS__)
#define trace_tplg(__e, ...)  trace_error(TRACE_CLASS_PIPE, __e, ##__VA_ARGS__)
#define trace_tplg_error(__e, ...)				\
	trace_error(TRACE_CLASS_PIPE, __e, ##__VA_ARGS__)

struct tplg_parser {
	void *pos;
	void *end;
};

struct sof_dai_types {
	const char *name;
	enum sof_ipc_dai_type type;
};

struct sof_effect_types {
	const char *name;
	enum sof_ipc_effect_type type;
};

static const struct sof_dai_types sof_dais[] = {
	{"SSP", SOF_DAI_INTEL_SSP},
	{"HDA", SOF_DAI_INTEL_HDA},
	{"DMIC", SOF_DAI_INTEL_DMIC},
};

static const struct sof_effect_types sof_effects[] = {
	{"EQFIR", SOF_EFFECT_INTEL_EQFIR},
	{"EQIIR", SOF_EFFECT_INTEL_EQIIR},
};

static enum sof_ipc_dai_type find_dai(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sof_dais); i++) {
		if (rstrcmp(name, sof_dais[i].name) == 0)
			return sof_dais[i].type;
	}

	return SOF_DAI_INTEL_NONE;
}

static enum sof_ipc_effect_type find_effect(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sof_effects); i++) {
		if (rstrcmp(name, sof_effects[i].name) == 0)
			return sof_effects[i].type;
	}

	return SOF_EFFECT_NONE;
}

static enum sof_ipc_frame find_format(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sof_frames); i++) {
		if (rstrcmp(name, sof_frames[i].name) == 0)
			return sof_frames[i].frame;
	}

	/* use s32le if nothing is specified */
	return SOF_IPC_FRAME_S32_LE;
}

static int get_token_uint32_t(void *elem, void *object, uint32_t offset,
			      uint32_t size)
{
	struct sof_tplg_vendor_value_elem *velem = elem;
	uint32_t *val = object + offset;

	*val = velem->value;
	return 0;
}

static int get_token_comp_format(void *elem, void *object, uint32_t offset,
				 uint32_t size)
{
	struct sof_tplg_vendor_string_elem *velem = elem;
	uint32_t *val = object + offset;

	*val = find_format(velem->string);
	return 0;
}

static int get_token_dai_type(void *elem, void *object, uint32_t offset,
			      uint32_t size)
{
	struct sof_tplg_vendor_string_elem *velem = elem;
	uint32_t *val = object + offset;

	*val = find_dai(velem->string);
	return 0;
}

static int get_token_effect_type(void *elem, void *object, uint32_t offset,
				 uint32_t size)
{
	struct sof_tplg_vendor_string_elem *velem = elem;
	uint32_t *val = object + offset;

	*val = find_effect(velem->string);
	return 0;
}

/* Buffers */
static const struct sof_topology_token buffer_tokens[] = {
	{SOF_TKN_BUF_SIZE, SOF_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
	 offsetof(struct sof_ipc_buffer, size), 0},
	{SOF_TKN_BUF_CAPS, SOF_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
	 offsetof(struct sof_ipc_buffer, caps), 0},
};

/* DAI */
static const struct sof_topology_token dai_tokens[] = {
	{SOF_TKN_DAI_DMAC_CONFIG, SOF_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_dai, dmac_config), 0},
	{SOF_TKN_DAI_TYPE, SOF_TPLG_TUPLE_TYPE_STRING, get_token_dai_type,
	 offsetof(struct sof_ipc_comp_dai, type), 0},
	{SOF_TKN_DAI_INDEX, SOF_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_dai, dai_index), 0},
	{SOF_TKN_DAI_DIRECTION, SOF_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_dai, direction), 0},
};

/* BE DAI link */
static const struct sof_topology_token dai_link_tokens[] = {
	{SOF_TKN_DAI_TYPE, SOF_TPLG_TUPLE_TYPE_STRING, get_token_dai_type,
	 offsetof(struct sof_ipc_dai_config, type), 0},
	{SOF_TKN_DAI_INDEX, SOF_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
	 offsetof(struct sof_ipc_dai_config, dai_index), 0},
};

/* scheduling */
static const struct sof_topology_token sched_tokens[] = {
	{SOF_TKN_SCHED_DEADLINE, SOF_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_pipe_new, deadline), 0},
	{SOF_TKN_SCHED_PRIORITY, SOF_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_pipe_new, priority), 0},
	{SOF_TKN_SCHED_MIPS, SOF_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_pipe_new, period_mips), 0},
	{SOF_TKN_SCHED_CORE, SOF_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_pipe_new, core), 0},
	{SOF_TKN_SCHED_FRAMES, SOF_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_pipe_new, frames_per_sched), 0},
	{SOF_TKN_SCHED_TIMER, SOF_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_pipe_new, timer_delay), 0},
};

/* volume */
static const struct sof_topology_token volume_tokens[] = {
	{SOF_TKN_VOLUME_RAMP_STEP_TYPE, SOF_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_volume, ramp), 0},
	{SOF_TKN_VOLUME_RAMP_STEP_MS,
	 SOF_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_volume, initial_ramp), 0},
};

/* SRC */
static const struct sof_topology_token src_tokens[] = {
	{SOF_TKN_SRC_RATE_IN, SOF_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_src, source_rate), 0},
	{SOF_TKN_SRC_RATE_OUT, SOF_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_src, sink_rate), 0},
};

/* Tone */
static const struct sof_topology_token tone_tokens[] = {
	{SOF_TKN_TONE_SAMPLE_RATE, SOF_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_tone, sample_rate), 0},
};

/* PCM */
static const struct sof_topology_token pcm_tokens[] = {
	{SOF_TKN_PCM_DMAC_CONFIG, SOF_TPLG_TUPLE_TYPE_WORD,
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_host, dmac_config), 0},
};

/* EFFECT */
static const struct sof_topology_token effect_tokens[] = {
	{SOF_TKN_EFFECT_TYPE, SOF_TPLG_TUPLE_TYPE_STRING,
	 get_token_effect_type,
	 offsetof(struct sof_ipc_comp_effect, type), 0},
};

/* Generic components */
static const struct sof_topology_token comp_tokens[] = {
	{SOF_TKN_COMP_PERIOD_SINK_COUNT,
	 SOF_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_config, periods_sink), 0},
	{SOF_TKN_COMP_PERIOD_SOURCE_COUNT,
	 SOF_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_config, periods_source), 0},
	{SOF_TKN_COMP_FORMAT,
	 SOF_TPLG_TUPLE_TYPE_STRING, get_token_comp_format,
	 offsetof(struct sof_ipc_comp_config, frame_fmt), 0},
	{SOF_TKN_COMP_PRELOAD_COUNT,
	 SOF_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_config, preload_count), 0},
};

static void parse_uuid_tokens(void *object,
			      const struct sof_topology_token *tokens,
			      int count, struct sof_tplg_vendor_array *array)
{
	struct sof_tplg_vendor_uuid_elem *elem;
	int i, j;

	/* parse element by element */
	for (i = 0; i < array->num_elems; i++) {
		elem = &array->uuid[i];

		/* search for token */
		for (j = 0; j < count; j++) {
			/* match token type */
			if (tokens[j].type != SOF_TPLG_TUPLE_TYPE_UUID)
				continue;

			/* match token id */
			if (tokens[j].token != elem->token)
				continue;

			/* matched - now load token */
			/* tracev_tplg("parse_uuid_tokens(), "
			 *	    "token = %d, uuid = %s",
			 *	    elem->token, (char *)elem->uuid);
			 */
			tracev_tplg("parse_uuid_tokens(), token = %d",
				    elem->token);
			tokens[j].get_token(elem, object, tokens[j].offset,
					    tokens[j].size);
		}
	}
}

static void parse_string_tokens(void *object,
				const struct sof_topology_token *tokens,
				int count,
				struct sof_tplg_vendor_array *array)
{
	struct sof_tplg_vendor_string_elem *elem;
	int i, j;

	/* parse element by element */
	for (i = 0; i < array->num_elems; i++) {
		elem = &array->string[i];

		/* search for token */
		for (j = 0; j < count; j++) {
			/* match token type */
			if (tokens[j].type != SOF_TPLG_TUPLE_TYPE_STRING)
				continue;

			/* match token id */
			if (tokens[j].token != elem->token)
				continue;

			/* matched - now load token */
			/* tracev_tplg("parse_string_tokens(), "
			 *	    "token = %d, string = %s",
			 *	    elem->token, elem->string);
			 */
			tracev_tplg("parse_string_tokens(), token = %d",
				    elem->token);
			tokens[j].get_token(elem, object, tokens[j].offset,
					    tokens[j].size);
		}
	}
}

static void parse_word_tokens(void *object,
			      const struct sof_topology_token *tokens,
			      int count, struct sof_tplg_vendor_array *array)
{
	struct sof_tplg_vendor_value_elem *elem;
	int i, j;

	/* parse element by element */
	for (i = 0; i < array->num_elems; i++) {
		elem = &array->value[i];

		/* search for token */
		for (j = 0; j < count; j++) {
			/* match token type */
			if (tokens[j].type != SOF_TPLG_TUPLE_TYPE_WORD)
				continue;

			/* match token id */
			if (tokens[j].token != elem->token)
				continue;

			/* matched - now load token */
			tracev_tplg("parse_word_tokens(), "
				    "token = %d, value = %d",
				    elem->token, elem->value);
			tokens[j].get_token(elem, object, tokens[j].offset,
					    tokens[j].size);
		}
	}
}

/* parse vendor tokens in topology */
static int parse_tokens(void *object, const struct sof_topology_token *tokens,
			int count, struct sof_tplg_vendor_array *array,
			int priv_size)
{
	int asize;

	while (priv_size > 0) {
		asize = array->size;

		/* validate asize */
		if (asize < 0) { /* FIXME: A zero-size array makes no sense */
			trace_tplg_error("error: invalid array size 0x%x",
					 asize);
			return -EINVAL;
		}

		/* make sure there is enough data before parsing */
		priv_size -= asize;
		if (priv_size < 0) {
			trace_tplg_error("error: invalid array size 0x%x",
					 asize);
			return -EINVAL;
		}

		/* call correct parser depending on type */
		switch (array->type) {
		case SOF_TPLG_TUPLE_TYPE_UUID:
			parse_uuid_tokens(object, tokens, count, array);
			break;
		case SOF_TPLG_TUPLE_TYPE_STRING:
			parse_string_tokens(object, tokens, count, array);
			break;
		case SOF_TPLG_TUPLE_TYPE_BOOL:
		case SOF_TPLG_TUPLE_TYPE_BYTE:
		case SOF_TPLG_TUPLE_TYPE_WORD:
		case SOF_TPLG_TUPLE_TYPE_SHORT:
			parse_word_tokens(object, tokens, count, array);
			break;
		default:
			trace_tplg_error("error: unknown token type %d",
					 array->type);
			return -EINVAL;
		}

		/* next array */
		array = (void *)array + asize;
	}
	return 0;
}

/* load pipeline graph DAPM widget*/
static int load_graph(struct ipc *ipc, struct tplg_parser *tplg,
		      struct comp_info *temp_comp_list,
		      int count, int num_comps, int pipeline_id)
{
	struct sof_ipc_pipe_comp_connect connection;
	struct sof_tplg_dapm_graph_elem *graph_elem;
	int i, j;

	/* set up component connections */
	memset(&connection, 0, sizeof(struct sof_ipc_pipe_comp_connect));
	connection.hdr.size = sizeof(struct sof_ipc_pipe_comp_connect);

	for (i = 0; i < count; i++) {
		graph_elem = (struct sof_tplg_dapm_graph_elem *)tplg->pos;
		tplg->pos += sizeof(struct sof_tplg_dapm_graph_elem);

		/* look up component id from the component list */
		connection.source_id = -1;
		connection.sink_id = -1;
		for (j = 0; j < num_comps; j++) {
			if (!rstrcmp(temp_comp_list[j].name,
				     graph_elem->source))
				connection.source_id = temp_comp_list[j].id;

			if (!rstrcmp(temp_comp_list[j].name, graph_elem->sink))
				connection.sink_id = temp_comp_list[j].id;
		}

		/* connect source and sink */
		if (connection.source_id != -1 && connection.sink_id != -1) {
			/* trace_tplg("Connect %s -> %s",
			 *	   temp_comp_list[connection.source_id].name,
			 *	   temp_comp_list[connection.sink_id].name);
			 */
			trace_tplg("Connect %d -> %d", connection.source_id,
				   connection.sink_id);

			if (ipc_comp_connect(ipc, &connection) < 0) {
				trace_tplg_error("error: comp connect");
				return -EINVAL;
			}
		} else {
			trace_tplg_error("Failed connection %d -> %d",
					 connection.source_id,
					 connection.sink_id);
		}
	}

	/* pipeline complete after pipeline connections are established */
	for (i = 0; i < num_comps; i++) {
		if (temp_comp_list[i].pipeline_id == pipeline_id &&
		    temp_comp_list[i].type == SOF_TPLG_DAPM_SCHEDULER)
			ipc_pipeline_complete(ipc, temp_comp_list[i].id);
	}

	return 0;
}

/* load buffer DAPM widget */
static int load_buffer(struct ipc *ipc, struct sof_tplg_dapm_widget *widget,
		       int comp_id, int pipeline_id)
{
	struct sof_ipc_buffer buffer;
	struct sof_tplg_private *private = &widget->priv;
	int ret;

	/* configure buffer */
	memset(&buffer, 0, sizeof(struct sof_ipc_buffer));
	buffer.comp.id = comp_id;
	buffer.comp.pipeline_id = pipeline_id;

	/* parse buffer tokens */
	ret = parse_tokens(&buffer, buffer_tokens,
			   ARRAY_SIZE(buffer_tokens), private->array,
			   private->size);
	if (ret) {
		trace_tplg("error: parse buffer tokens %d", private->size);
		return -EINVAL;
	}

	/* create buffer component */
	if (ipc_buffer_new(ipc, &buffer) < 0) {
		trace_tplg("error: buffer new");
		return -EINVAL;
	}

	return 0;
}

/* load pda dapm widget */
static int load_pga(struct ipc *ipc, struct sof_tplg_dapm_widget *widget,
		    int comp_id, int pipeline_id)
{
	struct sof_ipc_comp_volume volume;
	struct sof_tplg_private *private = &widget->priv;
	int ret;

	/* PGA needs 1 mixer type control to act as a trigger */
	if (widget->num_kcontrols != 1) {
		trace_tplg("error: invalid kcontrol count %d for pga",
			   widget->num_kcontrols);
		return -EINVAL;
	}

	/* configure volume */
	memset(&volume, 0, sizeof(struct sof_ipc_comp_volume));
	volume.comp.hdr.size = sizeof(struct sof_ipc_comp_volume);
	volume.comp.id = comp_id;
	volume.comp.type = SOF_COMP_VOLUME;
	volume.comp.pipeline_id = pipeline_id;
	volume.config.hdr.size = sizeof(struct sof_ipc_comp_config);

	/* parse volume tokens */
	ret = parse_tokens(&volume, volume_tokens,
			   ARRAY_SIZE(volume_tokens), private->array,
			   private->size);
	if (ret) {
		trace_tplg("error: parse volume tokens %d",
			   private->size);
		return -EINVAL;
	}

	/* parse component tokens */
	ret = parse_tokens(&volume.config, comp_tokens,
			   ARRAY_SIZE(comp_tokens), private->array,
			   private->size);
	if (ret) {
		trace_tplg("error: parse volume component tokens %d",
			   private->size);
		return -EINVAL;
	}

	/* load volume component */
	if (ipc_comp_new(ipc, (struct sof_ipc_comp *)&volume) < 0) {
		trace_tplg("error: comp register");
		return -EINVAL;
	}

	return 0;
}

/* load scheduler dapm widget */
static int load_pipeline(struct ipc *ipc,
			 struct sof_tplg_dapm_widget *widget,
			 struct sof_ipc_pipe_new *pipeline, int pipeline_id,
			 int sched_id, int comp_id)
{
	struct sof_tplg_private *private = &widget->priv;
	int ret;

	/* configure pipeline */
	pipeline->hdr.size = sizeof(struct sof_ipc_pipe_new);
	pipeline->pipeline_id = pipeline_id;
	pipeline->sched_id = sched_id;
	pipeline->comp_id = comp_id;

	/* parse scheduler tokens */
	ret = parse_tokens(pipeline, sched_tokens, ARRAY_SIZE(sched_tokens),
			   private->array, private->size);
	if (ret) {
		trace_tplg("error: parse pipeline tokens %d",
			   private->size);
		return -EINVAL;
	}

	/* Create pipeline */
	if (ipc_pipeline_new(ipc, pipeline) < 0) {
		trace_tplg("error: pipeline new");
		return -EINVAL;
	}

	return 0;
}

/* Load dapm widget kcontrols. We do not use controls at the moment but these
 * need to be parsed to keep tplg parsing in sync.
 */
static int load_controls(struct ipc *ipc, struct tplg_parser *tplg,
			 int num_kcontrols)
{
	struct sof_tplg_ctl_hdr *ctl_hdr;
	struct sof_tplg_mixer_control *mixer_ctl;
	struct sof_tplg_enum_control *enum_ctl;
	struct sof_tplg_bytes_control *bytes_ctl;
	int j;

	trace_tplg("load_controls()");

	for (j = 0; j < num_kcontrols; j++) {
		/* Get control header */
		ctl_hdr = (struct sof_tplg_ctl_hdr *)tplg->pos;

		/* load control based on type */
		switch (ctl_hdr->ops.info) {
		case SOF_TPLG_CTL_VOLSW:
		case SOF_TPLG_CTL_STROBE:
		case SOF_TPLG_CTL_VOLSW_SX:
		case SOF_TPLG_CTL_VOLSW_XR_SX:
		case SOF_TPLG_CTL_RANGE:
		case SOF_TPLG_DAPM_CTL_VOLSW:
			/* Get mixer type control */
			tracev_tplg("load_controls(), mixer_ctl");
			mixer_ctl = (struct sof_tplg_mixer_control *)
				tplg->pos;
			tplg->pos += sizeof(struct sof_tplg_mixer_control) +
				mixer_ctl->priv.size;
			break;
		case SOF_TPLG_CTL_ENUM:
		case SOF_TPLG_CTL_ENUM_VALUE:
		case SOF_TPLG_DAPM_CTL_ENUM_DOUBLE:
		case SOF_TPLG_DAPM_CTL_ENUM_VIRT:
		case SOF_TPLG_DAPM_CTL_ENUM_VALUE:
			/* Get enum type control */
			tracev_tplg("load_controls(), mixer_ctl");
			enum_ctl = (struct sof_tplg_enum_control *)
				tplg->pos;
			tplg->pos += sizeof(struct sof_tplg_enum_control) +
				enum_ctl->priv.size;
			break;
		case SOF_TPLG_CTL_BYTES:
			/* Get bytes type control */
			tracev_tplg("load_controls(), bytes_ctl");
			bytes_ctl = (struct sof_tplg_bytes_control *)
				tplg->pos;
			tplg->pos += sizeof(struct sof_tplg_bytes_control) +
				bytes_ctl->priv.size;
			break;
		default:
			trace_tplg("load_controls(), default");
			return -EINVAL;
		}
	}

	return 0;
}

/* load src dapm widget */
static int load_src(struct ipc *ipc, struct sof_tplg_dapm_widget *widget,
		    int comp_id, int pipeline_id)
{
	struct sof_ipc_comp_src src;
	struct sof_tplg_private *private = &widget->priv;
	int ret;

	/* configure src */
	memset(&src, 0, sizeof(struct sof_ipc_comp_src));
	src.comp.hdr.size = sizeof(struct sof_ipc_comp_src);
	src.comp.id = comp_id;
	src.comp.type = SOF_COMP_SRC;
	src.comp.pipeline_id = pipeline_id;
	src.config.hdr.size = sizeof(struct sof_ipc_comp_config);

	ret = parse_tokens(&src, src_tokens, ARRAY_SIZE(src_tokens),
			   private->array, private->size);
	if (ret) {
		trace_tplg("error: parse src tokens %d", private->size);
		return -EINVAL;
	}

	ret = parse_tokens(&src.config, comp_tokens,
			   ARRAY_SIZE(comp_tokens), private->array,
			   private->size);
	if (ret) {
		trace_tplg("error: parse src comp_tokens %d",
			   private->size);
		return -EINVAL;
	}

	/* load src component */
	if (ipc_comp_new(ipc, (struct sof_ipc_comp *)&src) < 0) {
		trace_tplg("error: new src comp");
		return -EINVAL;
	}

	return 0;
}

/* load iir dapm widget */
static int load_iir(struct ipc *ipc, struct sof_tplg_dapm_widget *widget,
		    int comp_id, int pipeline_id)
{
	trace_tplg("load_iir()");

	struct sof_ipc_comp_eq_iir *iir;
	struct sof_tplg_private *private = &widget->priv;
	size_t iir_data_size = private->size - SOF_EFFECT_DATA_SIZE;
	void *iir_data_p = private->data + SOF_EFFECT_DATA_SIZE;
	size_t size = sizeof(struct sof_ipc_comp_eq_iir) + iir_data_size;
	int ret;

	iir = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, size);
	if (!iir)
		return -ENOMEM;

	iir->comp.hdr.size = size;
	iir->comp.id = comp_id;
	iir->comp.type = SOF_COMP_EQ_IIR;
	iir->comp.pipeline_id = pipeline_id;
	iir->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	memcpy(&iir->data, iir_data_p, iir_data_size);

	ret = parse_tokens(&iir->config, comp_tokens,
			   ARRAY_SIZE(comp_tokens), private->array,
			   private->size);
	if (ret) {
		trace_tplg("error: parse iir.cfg tokens failed %d",
			   private->size);
		goto err;
	}

	/* load EQ component */
	ret = ipc_comp_new(ipc, (struct sof_ipc_comp *)iir);
	if (ret) {
		trace_tplg("error: new iir comp");
		goto err;
	}

err:
	rfree(iir);
	return ret;
}

/* load fir dapm widget */
static int load_fir(struct ipc *ipc, struct sof_tplg_dapm_widget *widget,
		    int comp_id, int pipeline_id)
{
	trace_tplg("load_fir()");

	struct sof_ipc_comp_eq_fir *fir;
	struct sof_tplg_private *private = &widget->priv;
	size_t fir_data_size = private->size - SOF_EFFECT_DATA_SIZE;
	void *fir_data_p = private->data + SOF_EFFECT_DATA_SIZE;
	size_t size = sizeof(struct sof_ipc_comp_eq_fir) + fir_data_size;
	int ret;

	fir = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, size);
	if (!fir)
		return -ENOMEM;

	memset(fir, 0, size);
	fir->comp.hdr.size = size;
	fir->comp.id = comp_id;
	fir->comp.type = SOF_COMP_EQ_FIR;
	fir->comp.pipeline_id = pipeline_id;
	fir->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	memcpy(&fir->data, fir_data_p, fir_data_size);

	ret = parse_tokens(&fir->config, comp_tokens,
			   ARRAY_SIZE(comp_tokens), private->array,
			   private->size);
	if (ret) {
		trace_tplg("error: parse fir.cfg tokens failed %d",
			   private->size);
		goto err;
	}

	/* load EQ component */
	ret = ipc_comp_new(ipc, (struct sof_ipc_comp *)fir);
	if (ret) {
		trace_tplg("error: new fir comp");
		goto err;
	}

err:
	rfree(fir);
	return ret;
}

/* load effect dapm widget */
static int load_effect(struct ipc *ipc, struct sof_tplg_dapm_widget *widget,
		       int comp_id, int pipeline_id)
{
	struct sof_ipc_comp_effect effect = { {0} };
	struct sof_tplg_private *private = &widget->priv;
	int ret;

	/* check we have some tokens - we need at least effect type */
	if (private->size == 0) {
		trace_tplg("error: effect tokens not found");
		return -EINVAL;
	}

	ret = parse_tokens(&effect, effect_tokens,
			   ARRAY_SIZE(effect_tokens),
			   private->array, private->size);
	if (ret) {
		trace_tplg("error: parse effect tokens %d", private->size);
		return -EINVAL;
	}

	/* now load effect specific data and send IPC */
	switch (effect.type) {
	case SOF_EFFECT_INTEL_EQFIR:
		ret = load_fir(ipc, widget, comp_id, pipeline_id);
		break;
	case SOF_EFFECT_INTEL_EQIIR:
		ret = load_iir(ipc, widget, comp_id, pipeline_id);
		break;
	default:
		trace_tplg("error: invalid effect type %d",
			   effect.type);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		trace_tplg("error: effect loading failed");
		return ret;
	}

	return 0;
}

/* load tone dapm widget */
static int load_siggen(struct ipc *ipc, struct sof_tplg_dapm_widget *widget,
		       int comp_id, int pipeline_id)
{
	struct sof_ipc_comp_tone tone;
	struct sof_tplg_private *private = &widget->priv;
	int ret;

	/* siggen needs 1 mixer type control to act as a trigger */
	if (widget->num_kcontrols != 1) {
		trace_tplg("error: invalid kcontrol count %d for siggen",
			   widget->num_kcontrols);
		return -EINVAL;
	}

	/* configure tone */
	memset(&tone, 0, sizeof(struct sof_ipc_comp_tone));
	tone.comp.hdr.size = sizeof(struct sof_ipc_comp_tone);
	tone.comp.id = comp_id;
	tone.comp.type = SOF_COMP_TONE;
	tone.comp.pipeline_id = pipeline_id;
	tone.config.hdr.size = sizeof(struct sof_ipc_comp_config);

	/* parse tone tokens */
	ret = parse_tokens(&tone, tone_tokens, ARRAY_SIZE(tone_tokens),
			   private->array, private->size);
	if (ret) {
		trace_tplg("error: parse tone tokens %d", private->size);
		return -EINVAL;
	}

	/* parse comp tokens */
	ret = parse_tokens(&tone.config, comp_tokens,
			   ARRAY_SIZE(comp_tokens), private->array,
			   private->size);
	if (ret) {
		trace_tplg("error: parse tone comp_tokens %d",
			   private->size);
		return -EINVAL;
	}

	/* load tone component */
	if (ipc_comp_new(ipc, (struct sof_ipc_comp *)&tone) < 0) {
		trace_tplg("error: new tone comp");
		return -EINVAL;
	}

	return 0;
}

/* load DAI dapm widget */
static int load_dai(struct ipc *ipc, struct sof_tplg_dapm_widget *widget,
		    int comp_id, int pipeline_id)
{
	struct sof_ipc_comp_dai comp_dai;
	struct sof_tplg_private *private = &widget->priv;
	int ret;

	/* configure dai */
	memset(&comp_dai, 0, sizeof(struct sof_ipc_comp_dai));
	comp_dai.comp.hdr.size = sizeof(comp_dai);
	comp_dai.comp.id = comp_id;
	comp_dai.comp.type = SOF_COMP_DAI;
	comp_dai.comp.pipeline_id = pipeline_id;
	comp_dai.config.hdr.size = sizeof(struct sof_ipc_comp_config);

	ret = parse_tokens(&comp_dai, dai_tokens, ARRAY_SIZE(dai_tokens),
			   private->array, private->size);
	if (ret) {
		trace_tplg("error: parse dai tokens failed %d",
			   private->size);
		return -EINVAL;
	}

	ret = parse_tokens(&comp_dai.config, comp_tokens,
			   ARRAY_SIZE(comp_tokens),
			   private->array, private->size);
	if (ret) {
		trace_tplg("error: parse dai component tokens failed %d",
			   private->size);
		return -EINVAL;
	}

	/* load dai component */
	if (ipc_comp_new(ipc, (struct sof_ipc_comp *)&comp_dai) < 0) {
		trace_tplg("error: new DAI comp");
		return -EINVAL;
	}

	return 0;
}

/* load DAI dapm widget */
static int load_pcm(struct ipc *ipc, struct sof_tplg_dapm_widget *widget,
		    int comp_id, int pipeline_id,
		    enum sof_ipc_stream_direction dir)
{
	struct sof_ipc_comp_host host;
	struct sof_tplg_private *private = &widget->priv;
	int ret;

	memset(&host, 0, sizeof(struct sof_ipc_comp_host));
	host.comp.hdr.size = sizeof(host);
	host.comp.id = comp_id;
	host.comp.type = SOF_COMP_HOST;
	host.comp.pipeline_id = pipeline_id;
	host.config.hdr.size = sizeof(struct sof_ipc_comp_config);
	host.direction = dir;

	ret = parse_tokens(&host, pcm_tokens, ARRAY_SIZE(pcm_tokens),
			   private->array, private->size);
	if (ret) {
		trace_tplg("error: parse host tokens failed %d",
			   private->size);
		return -EINVAL;
	}

	ret = parse_tokens(&host.config, comp_tokens,
			   ARRAY_SIZE(comp_tokens), private->array,
			   private->size);
	if (ret) {
		trace_tplg("error: parse host.cfg tokens failed %d",
			   private->size);
		return -EINVAL;
	}

	/* load host component */
	if (ipc_comp_new(ipc, (struct sof_ipc_comp *)&host) < 0) {
		trace_tplg("error: new src comp");
		return -EINVAL;
	}

	return 0;
}

/* load dapm widget */
static int load_widget(struct ipc *ipc,
		       struct tplg_parser *tplg,
		       struct comp_info *comp_list,
		       struct sof_ipc_pipe_new *pipeline,
		       int pipeline_id,
		       int sched_id,
		       int comp_id,
		       int comp_index)
{
	struct sof_tplg_dapm_widget *widget;
	size_t name_size;
	int ret;

	/* Get pointer to widget and advance to next */
	widget = (struct sof_tplg_dapm_widget *)tplg->pos;
	tplg->pos += widget->size + widget->priv.size;

	/* create a list with all widget info
	 * containing mapping between component names and ids
	 * which will be used for setting up component connections
	 */
	comp_list[comp_index].id = comp_id;
	comp_list[comp_index].type = widget->id;
	comp_list[comp_index].pipeline_id = pipeline_id;
	//comp_list[comp_index].name = strdup(widget->name);
	name_size = rstrlen(widget->name);
	comp_list[comp_index].name = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
					     name_size);
	memcpy(comp_list[comp_index].name, widget->name, name_size);

	/* TODO: Cannot yet print strings
	 * trace_tplg("loading widget %s id %d", comp_list[comp_index].name,
	 *	      comp_list[comp_index].id);
	 */

	/* load widget based on type */
	switch (comp_list[comp_index].type) {
	case SOF_TPLG_DAPM_SCHEDULER:
		trace_tplg("load_widget(), SCHEDULER (%d)", comp_id);
		ret = load_pipeline(ipc, widget, pipeline, pipeline_id,
				    sched_id, comp_id);
		if (ret < 0) {
			trace_tplg("error: load pipeline");
			return -EINVAL;
		}
		break;
	case SOF_TPLG_DAPM_BUFFER:
		trace_tplg("load_widget(), BUFFER (%d)", comp_id);
		ret = load_buffer(ipc, widget, comp_id, pipeline_id);
		if (ret < 0) {
			trace_tplg("error: load buffer");
			return -EINVAL;
		}
		break;
	case SOF_TPLG_DAPM_PGA:
		trace_tplg("load_widget(), PGA (%d)", comp_id);
		ret = load_pga(ipc, widget, comp_id, pipeline_id);
		if (ret < 0) {
			trace_tplg("error: load pga");
			return -EINVAL;
		}
		break;
	case SOF_TPLG_DAPM_AIF_IN:
		trace_tplg("load_widget(), AIF_IN (%d)", comp_id);
		ret = load_pcm(ipc, widget, comp_id, pipeline_id,
			       SOF_IPC_STREAM_PLAYBACK);
		if (ret < 0) {
			trace_tplg("error: load pcm");
			return -EINVAL;
		}
		break;
	case SOF_TPLG_DAPM_AIF_OUT:
		trace_tplg("load_widget(), AIF_OUT (%d)", comp_id);
		ret = load_pcm(ipc, widget, comp_id, pipeline_id,
			       SOF_IPC_STREAM_CAPTURE);
		if (ret < 0) {
			trace_tplg("error: load pcm");
			return -EINVAL;
		}
		break;
	case SOF_TPLG_DAPM_DAI_IN:
	case SOF_TPLG_DAPM_DAI_OUT:
		trace_tplg("load_widget(), DAI (%d)", comp_id);
		ret = load_dai(ipc, widget, comp_id, pipeline_id);
		if (ret < 0) {
			trace_tplg("error: load dai");
			return -EINVAL;
		}
		break;
	case SOF_TPLG_DAPM_SRC:
		trace_tplg("load_widget(), SRC (%d)", comp_id);
		ret = load_src(ipc, widget, comp_id, pipeline_id);
		if (ret < 0) {
			trace_tplg("error: load src");
			return -EINVAL;
		}
		break;
	case SOF_TPLG_DAPM_SIGGEN:
		trace_tplg("load_widget(), SIGGEN (%d)", comp_id);
		ret = load_siggen(ipc, widget, comp_id, pipeline_id);
		if (ret < 0) {
			trace_tplg("error: load siggen");
			return -EINVAL;
		}
		break;
	case SOF_TPLG_DAPM_EFFECT:
		trace_tplg("load_widget(), EFFECT (%d)", comp_id);
		ret = load_effect(ipc, widget, comp_id, pipeline_id);
		if (ret < 0) {
			trace_tplg("error: load src");
			return -EINVAL;
		}
		break;
	default:
		trace_tplg("info: Widget type not supported %d",
			   widget->id);
		break;
	}

	/* If the widget has kcontrols need to parse them to stay in sync
	 * in parsing.
	 */
	if (widget->num_kcontrols > 0) {
		tracev_tplg("load_widget(), num_kcontrols = %d",
			    widget->num_kcontrols);
		ret = load_controls(ipc, tplg, widget->num_kcontrols);
		if (ret < 0) {
			trace_tplg("error: load controls");
			return -EINVAL;
		}
	}

	return 0;
}

/* parse topology file and set up pipeline */
int tplg_parse(struct ipc *ipc, uint8_t *tplg_bytes, size_t tplg_size)
{
	struct sof_tplg_hdr *hdr;
	struct comp_info *comp_list = NULL;
	struct sof_ipc_pipe_new pipeline;
	struct tplg_parser tplg;
	int comp_id = 0;
	int comp_num = 0;
	int i;
	int sched_id = 0;
	size_t size;
	int ret;

	tplg.pos = tplg_bytes;
	tplg.end = tplg_bytes + tplg_size;
	while (tplg.pos != tplg.end) {
		/* read topology header */
		hdr = (struct sof_tplg_hdr *)tplg.pos;
		tplg.pos += sizeof(struct sof_tplg_hdr);

		trace_tplg("tplg_parse(), "
			   "type = %d, size = %d, count = %d, index = %d",
			   hdr->type, hdr->payload_size, hdr->count,
			   hdr->index);

		/* parse header and load the next block based on type */
		switch (hdr->type) {
			/* load dapm widget */
		case SOF_TPLG_TYPE_DAPM_WIDGET:
			trace_tplg("tplg_parse(), DAPM_WIDGET, count = %d",
				   hdr->count);
			size = sizeof(struct comp_info) * hdr->count;
			comp_list = (struct comp_info *)
				rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, size);
			comp_num = hdr->count;

			for (i = 0; i < comp_num; i++) {
				ret = load_widget(ipc, &tplg, comp_list,
						  &pipeline, hdr->index,
						  sched_id, comp_id++, i);
			}
			break;
			/* set up component connections from pipeline graph */
		case SOF_TPLG_TYPE_DAPM_GRAPH:
			trace_tplg("tplg_parse(), DAPM_GRAPH");
			ret = load_graph(ipc, &tplg, comp_list, hdr->count,
					 comp_num, hdr->index);
			if (ret < 0) {
				trace_tplg("error: pipeline graph");
				return -EINVAL;
			}
			break;
		case SOF_TPLG_TYPE_MIXER:
			trace_tplg("tplg_parse(), MIXER");
			tplg.pos += hdr->payload_size;
			break;
		case SOF_TPLG_TYPE_BYTES:
			trace_tplg("tplg_parse(), BYTES");
			tplg.pos += hdr->payload_size;
			break;
		case SOF_TPLG_TYPE_ENUM:
			trace_tplg("tplg_parse(), ENUM");
			tplg.pos += hdr->payload_size;
			break;
		case SOF_TPLG_TYPE_DAI_LINK:
			trace_tplg("tplg_parse(), DAI_LINK");
			tplg.pos += hdr->payload_size;
			break;
		case SOF_TPLG_TYPE_PCM:
			trace_tplg("tplg_parse(), PCM");
			tplg.pos += hdr->payload_size;
			break;
		case SOF_TPLG_TYPE_MANIFEST:
			trace_tplg("tplg_parse(), MANIFEST");
			tplg.pos += hdr->payload_size;
			break;
		case SOF_TPLG_TYPE_CODEC_LINK:
			trace_tplg("tplg_parse(), CODEC_LINK");
			tplg.pos += hdr->payload_size;
			break;
		case SOF_TPLG_TYPE_BACKEND_LINK:
			trace_tplg("tplg_parse(), BACKEND_LINK");
			tplg.pos += hdr->payload_size;
			break;
		case SOF_TPLG_TYPE_PDATA:
			trace_tplg("tplg_parse(), PDATA");
			tplg.pos += hdr->payload_size;
			break;
		case SOF_TPLG_TYPE_DAI:
			trace_tplg("tplg_parse(), DAI");
			tplg.pos += hdr->payload_size;
			break;
		case SOF_TPLG_TYPE_VENDOR_FW:
			trace_tplg("tplg_parse(), VENDOR_FW");
			tplg.pos += hdr->payload_size;
			break;
		case SOF_TPLG_TYPE_VENDOR_CONFIG:
			trace_tplg("tplg_parse(), VENDOR_CONFIG");
			tplg.pos += hdr->payload_size;
			break;
		case SOF_TPLG_TYPE_VENDOR_COEFF:
			trace_tplg("tplg_parse(), VENDOR_COEFF");
			tplg.pos += hdr->payload_size;
			break;
		case SOF_TPLG_TYPE_VENDOR_CODEC:
			trace_tplg("tplg_parse(), VENDOR_CODEC");
			tplg.pos += hdr->payload_size;
			break;
		default:
			trace_tplg_error("Error: Unknown type.");
			tplg.pos += hdr->payload_size;
			break;
		}
	}

	for (i = 0; i < comp_num; i++)
		rfree(comp_list[i].name);

	rfree(comp_list);
	trace_tplg("tplg_parse(), Done.");
	return 0;
}

#else

/* Return an error if topology parsing was called without having the
 * feature enabled in firmware configuration.
 */
int tplg_parse(struct ipc *ipc, uint8_t *tplg_bytes, size_t tplg_size)
{
	return -EINVAL;
}

#endif /* defined(CONFIG_TOPOLOGY) */
