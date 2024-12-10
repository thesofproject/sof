/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017-2024 Intel Corporation.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_AUDIO_SRC_SRC_COMMON_H__
#define __SOF_AUDIO_SRC_SRC_COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <ipc4/base-config.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include "src_ipc.h"

struct src_stage {
	const int idm;
	const int odm;
	const int num_of_subfilters;
	const int subfilter_length;
	const int filter_length;
	const int blk_in;
	const int blk_out;
	const int halfband;
	const int shift;
	const void *coefs; /* Can be int16_t or int32_t depending on config */
};

struct src_param {
	int fir_s1;
	int fir_s2;
	int out_s1;
	int out_s2;
	int sbuf_length;
	int src_multich;
	int total;
	int blk_in;
	int blk_out;
	int stage1_times;
	int stage2_times;
	int idx_in;
	int idx_out;
	int num_in_fs;
	int num_out_fs;
	int max_fir_delay_size_xnch;
	int max_out_delay_size_xnch;
	int nch;
	const struct src_stage *stage1;
	const struct src_stage *stage2;
	const int *in_fs;
	const int *out_fs;
};

struct src_state {
	int fir_delay_size;	/* samples */
	int out_delay_size;	/* samples */
	int32_t *fir_delay;
	int32_t *out_delay;
	int32_t *fir_wp;
	int32_t *out_rp;
};

struct polyphase_src {
	int number_of_stages;
	const struct src_stage *stage1;
	const struct src_stage *stage2;
	struct src_state state1;
	struct src_state state2;
};

struct src_stage_prm {
	int nch;
	int times;
	void const *x_rptr;
	void const *x_end_addr;
	size_t x_size;
	void *y_wptr;
	void *y_addr;
	void *y_end_addr;
	size_t y_size;
	int shift;
	struct src_state *state;
	const struct src_stage *stage;
};

static inline void src_inc_wrap(int32_t **ptr, int32_t *end, size_t size)
{
	if (*ptr >= end)
		*ptr = (int32_t *)((uint8_t *)*ptr - size);
}

static inline void src_dec_wrap(int32_t **ptr, int32_t *addr, size_t size)
{
	if (*ptr < addr)
		*ptr = (int32_t *)((uint8_t *)*ptr + size);
}

#if CONFIG_FORMAT_S16LE
static inline void src_inc_wrap_s16(int16_t **ptr, int16_t *end, size_t size)
{
	if (*ptr >= end)
		*ptr = (int16_t *)((uint8_t *)*ptr - size);
}

static inline void src_dec_wrap_s16(int16_t **ptr, int16_t *addr, size_t size)
{
	if (*ptr < addr)
		*ptr = (int16_t *)((uint8_t *)*ptr + size);
}
#endif /* CONFIG_FORMAT_S16LE */

static inline void src_state_reset(struct src_state *state)
{
	state->fir_delay_size = 0;
	state->out_delay_size = 0;
}

static inline void src_polyphase_reset(struct polyphase_src *src)
{
	src->number_of_stages = 0;
	src->stage1 = NULL;
	src->stage2 = NULL;
	src_state_reset(&src->state1);
	src_state_reset(&src->state2);
}

int src_polyphase(struct polyphase_src *src, int32_t x[], int32_t y[],
		  int n_in);

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
void src_polyphase_stage_cir(struct src_stage_prm *s);
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
void src_polyphase_stage_cir_s16(struct src_stage_prm *s);
#endif /* CONFIG_FORMAT_S16LE */

int32_t src_input_rates(void);

int32_t src_output_rates(void);

void src_set_alignment(struct sof_source *source, struct sof_sink *sink);

struct comp_data {
#if CONFIG_IPC_MAJOR_4
	struct ipc4_config_src ipc_config;
#else
	struct ipc_config_src ipc_config;
#endif /* CONFIG_IPC_MAJOR_4 */
	struct polyphase_src src;
	struct src_param param;
	int32_t *delay_lines;
	uint32_t sink_rate;
	uint32_t source_rate;
	int32_t *sbuf_w_ptr;
	int32_t const *sbuf_r_ptr;
	int sbuf_avail;
	int data_shift;
	int source_frames;
	int sink_frames;
	int sample_container_bytes;
	int channels_count;
	int (*src_func)(struct comp_data *cd, struct sof_source *source,
			struct sof_sink *sink);
	void (*polyphase_func)(struct src_stage_prm *s);
};

#if CONFIG_IPC_MAJOR_4

int src_stream_pcm_source_rate_check(struct ipc4_config_src cfg,
				     struct sof_ipc_stream_params *params);
int src_stream_pcm_sink_rate_check(struct ipc4_config_src cfg,
				   struct sof_ipc_stream_params *params);
#elif CONFIG_IPC_MAJOR_3
int src_stream_pcm_sink_rate_check(struct ipc_config_src cfg,
				   struct sof_ipc_stream_params *params);
int src_stream_pcm_source_rate_check(struct ipc_config_src cfg,
				     struct sof_ipc_stream_params *params);
#endif /* CONFIG_IPC_MAJOR_4 */

/* Calculates the needed FIR delay line length */
static inline int src_fir_delay_length(const struct src_stage *s)
{
	return s->subfilter_length + (s->num_of_subfilters - 1) * s->idm
		+ s->blk_in;
}

/* Calculates the FIR output delay line length */
static inline int src_out_delay_length(const struct src_stage *s)
{
	return 1 + (s->num_of_subfilters - 1) * s->odm;
}

/* Returns index of a matching sample rate */
static inline int src_find_fs(const int *fs_list, int list_length, int fs)
{
	int i;

	for (i = 0; i < list_length; i++) {
		if (fs_list[i] == fs)
			return i;
	}
	return -EINVAL;
}

/* Fallback function */
static inline int src_fallback(struct comp_data *cd,
			       struct sof_source *source,
			       struct sof_sink *sink)
{
	return 0;
}

int src_allocate_copy_stages(struct comp_dev *dev, struct src_param *prm,
			     const struct src_stage *stage_src1,
			     const struct src_stage *stage_src2);
int src_rate_check(const void *spec);
int src_set_params(struct processing_module *mod, struct sof_sink *sink);

void src_get_source_sink_params(struct comp_dev *dev, struct sof_source *source,
				struct sof_sink *sink);
int src_prepare_general(struct processing_module *mod,
			struct sof_source *source,
			struct sof_sink *sink);
int src_init(struct processing_module *mod);

int src_copy_sxx(struct comp_data *cd, struct sof_source *source,
		 struct sof_sink *sink);
int src_params_general(struct processing_module *mod,
		       struct sof_source *source,
		       struct sof_sink *sink);
int src_param_set(struct comp_dev *dev, struct comp_data *cd);

bool src_is_ready_to_process(struct processing_module *mod,
			     struct sof_source **sources, int num_of_sources,
			     struct sof_sink **sinks, int num_of_sinks);
int src_process(struct processing_module *mod,
		struct sof_source **sources, int num_of_sources,
		struct sof_sink **sinks, int num_of_sinks);

int src_set_config(struct processing_module *mod, uint32_t config_id,
		   enum module_cfg_fragment_position pos, uint32_t data_offset_size,
		   const uint8_t *fragment, size_t fragment_size, uint8_t *response,
		   size_t response_size);
int src_get_config(struct processing_module *mod, uint32_t config_id,
		   uint32_t *data_offset_size, uint8_t *fragment, size_t fragment_size);
int src_free(struct processing_module *mod);
int src_reset(struct processing_module *mod);
extern struct tr_ctx src_tr;

#ifdef CONFIG_IPC_MAJOR_4
#define SRC_UUID src4_uuid
#else
#define SRC_UUID src_uuid
#endif
extern const struct sof_uuid SRC_UUID;

#endif /* __SOF_AUDIO_SRC_SRC_COMMON_H__ */
