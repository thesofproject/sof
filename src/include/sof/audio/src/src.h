/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_AUDIO_SRC_SRC_H__
#define __SOF_AUDIO_SRC_SRC_H__

#include <stddef.h>
#include <stdint.h>
#include <ipc4/base-config.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>

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
	int nch;
};

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
	struct src_stage *stage1;
	struct src_stage *stage2;
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
	struct src_stage *stage;
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

void src_polyphase_reset(struct polyphase_src *src);

int src_polyphase_init(struct polyphase_src *src, struct src_param *p,
		       int32_t *delay_lines_start);

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

#if CONFIG_IPC_MAJOR_4
/* src component private data */
struct ipc4_config_src {
	struct ipc4_base_module_cfg base;
	uint32_t sink_rate;
};
#endif

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

#endif /* __SOF_AUDIO_SRC_SRC_H__ */

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

int src_rate_check(const void *spec);
int src_set_params(struct processing_module *mod, struct sof_sink *sink);

void src_get_source_sink_params(struct comp_dev *dev, struct sof_source *source,
				struct sof_sink *sink);
int src_prepare_general(struct processing_module *mod,
			struct sof_source *source,
			struct sof_sink *sink);
int src_init(struct processing_module *mod);
int src_fallback(struct comp_data *cd, struct sof_source *source,
		 struct sof_sink *sink);

extern const struct sof_uuid src_uuid;
extern struct tr_ctx src_tr;

