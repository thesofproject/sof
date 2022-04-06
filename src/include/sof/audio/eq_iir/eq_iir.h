/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_AUDIO_EQ_IIR_EQ_IIR_H__
#define __SOF_AUDIO_EQ_IIR_EQ_IIR_H__

#include <stdint.h>
#include <sof/math/iir_df2t.h>

/** \brief Macros to convert without division bytes count to samples count */
#define EQ_IIR_BYTES_TO_S16_SAMPLES(b)	((b) >> 1)
#define EQ_IIR_BYTES_TO_S32_SAMPLES(b)	((b) >> 2)

struct audio_stream;
struct comp_dev;

/** \brief Type definition for processing function select return value. */
typedef void (*eq_iir_func)(const struct comp_dev *dev,
			    const struct audio_stream __sparse_cache *source,
			    struct audio_stream __sparse_cache *sink,
			    uint32_t frames);

/** \brief IIR EQ processing functions map item. */
struct eq_iir_func_map {
	uint8_t source;				/**< source frame format */
	uint8_t sink;				/**< sink frame format */
	eq_iir_func func;			/**< processing function */
};

#ifdef UNIT_TEST
void sys_comp_eq_iir_init(void);
#endif

#endif /* __SOF_AUDIO_EQ_IIR_EQ_IIR_H__ */
