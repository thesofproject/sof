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
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/* A full 22th order equalizer with 11 biquads cover octave bands 1-11 in
 * in the 0 - 20 kHz bandwidth.
 */
#define IIR_DF2T_BIQUADS_MAX 11

struct iir_state_df2t {
	int mute; /* Set to 1 to mute EQ output, 0 otherwise */
	int biquads; /* Number of IIR 2nd order sections total */
	int biquads_in_series; /* Number of IIR 2nd order sections in series*/
	int32_t *coef; /* Pointer to IIR coefficients */
	int64_t *delay; /* Pointer to IIR delay line */
};

#define NHEADER_DF2T 2

struct iir_header_df2t {
	int32_t num_sections;
	int32_t num_sections_in_series;
};

#define NBIQUAD_DF2T 7

struct iir_biquad_df2t {
	int32_t a2; /* Q2.30 */
	int32_t a1; /* Q2.30 */
	int32_t b2; /* Q2.30 */
	int32_t b1; /* Q2.30 */
	int32_t b0; /* Q2.30 */
	int32_t output_shift; /* Number of right shifts */
	int32_t output_gain;  /* Q2.14 */
};

int32_t iir_df2t(struct iir_state_df2t *iir, int32_t x);

size_t iir_init_coef_df2t(struct iir_state_df2t *iir, int32_t config[]);

void iir_init_delay_df2t(struct iir_state_df2t *iir, int64_t **delay);

void iir_mute_df2t(struct iir_state_df2t *iir);

void iir_unmute_df2t(struct iir_state_df2t *iir);

void iir_reset_df2t(struct iir_state_df2t *iir);
