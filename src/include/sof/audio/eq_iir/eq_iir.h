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

struct comp_buffer;
struct comp_dev;

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

#endif /* __SOF_AUDIO_EQ_IIR_EQ_IIR_H__ */
