/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_AUDIO_EQ_IIR_H__
#define __SOF_AUDIO_EQ_IIR_H__

#include <arch/audio/eq_iir.h>
#include <stddef.h>
#include <stdint.h>

struct comp_buffer;
struct comp_dev;
struct sof_eq_iir_header_df2t;

#define IIR_DF2T_NUM_DELAYS 2

/** \brief IIR EQ processing functions map item. */
struct eq_iir_func_map {
	uint8_t source;				/**< source frame format */
	uint8_t sink;				/**< sink frame format */
	void (*func)(struct comp_dev *dev,	/**< EQ processing function */
		     struct comp_buffer *source,
		     struct comp_buffer *sink,
		     uint32_t frames);
};

/** \brief Type definition for processing function select return value. */
typedef void (*eq_iir_func)(struct comp_dev *dev,
			    struct comp_buffer *source,
			    struct comp_buffer *sink,
			    uint32_t frames);

struct iir_state_df2t {
	unsigned int biquads; /* Number of IIR 2nd order sections total */
	unsigned int biquads_in_series; /* Number of IIR 2nd order sections
					 * in series.
					 */
	int32_t *coef; /* Pointer to IIR coefficients */
	int64_t *delay; /* Pointer to IIR delay line */
};

int32_t iir_df2t(struct iir_state_df2t *iir, int32_t x);

size_t iir_init_coef_df2t(struct iir_state_df2t *iir,
			  struct sof_eq_iir_header_df2t *config);

void iir_init_delay_df2t(struct iir_state_df2t *iir, int64_t **delay);

void iir_mute_df2t(struct iir_state_df2t *iir);

void iir_unmute_df2t(struct iir_state_df2t *iir);

void iir_reset_df2t(struct iir_state_df2t *iir);

#endif /* __SOF_AUDIO_EQ_IIR_H__ */
