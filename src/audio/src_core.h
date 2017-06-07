/*
 * Copyright (c) 2017, Intel Corporation
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
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *
 */

#ifndef SRC_CORE_H
#define SRC_CORE_H

#define MAX(a,b) ((a) > (b)) ? (a) : (b)

/* Include SRC min/max constants etc. */
#include <reef/audio/coefficients/src/src_int24_define.h>

struct src_alloc {
	int fir_s1;
	int fir_s2;
	int out_s1;
	int out_s2;
	int scratch;
	int single_src;
	int total;
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
	const int32_t *coefs;
};

struct src_state {
	int fir_delay_size;
	int out_delay_size;
	int fir_wi;
	int fir_ri;
	int out_wi;
	int out_ri;
	int32_t *fir_delay;
	int32_t *out_delay;
};

struct polyphase_src {
	int mute;
	int number_of_stages;
	int blk_in;
	int blk_out;
	int stage1_times;
	int stage2_times;
	struct src_stage *stage1;
	struct src_stage *stage2;
	struct src_state state1;
	struct src_state state2;
};

struct src_stage_prm {
	int times;
	int32_t *x_rptr;
	int32_t *x_end_addr;
	size_t x_size;
	int x_inc;
	int32_t *y_wptr;
	int32_t *y_end_addr;
	size_t y_size;
	int y_inc;
	struct src_state *state;
	struct src_stage *stage;
};

static inline void src_polyphase_mute(struct polyphase_src *src)
{
	src->mute = 1;
}

static inline void src_polyphase_unmute(struct polyphase_src *src)
{
	src->mute = 0;
}

static inline int src_polyphase_getmute(struct polyphase_src *src)
{
	return src->mute;
}

static inline int src_polyphase_get_blk_in(struct polyphase_src *src)
{
	return src->blk_in;
}

static inline int src_polyphase_get_blk_out(struct polyphase_src *src)
{
	return src->blk_out;
}

void src_polyphase_reset(struct polyphase_src *src);

int src_polyphase_init(struct polyphase_src *src, int fs1, int fs2, int32_t *delay_lines_start);

int src_polyphase(struct polyphase_src *src, int32_t x[], int32_t y[], int n_in);

void src_polyphase_stage_cir(struct src_stage_prm *s);

int src_buffer_lengths(struct src_alloc *a, int fs_in, int fs_out, int nch);

int32_t src_input_rates();

int32_t src_output_rates();

#ifdef MODULE_TEST
void src_print_info(struct polyphase_src *src);
#endif

#endif
