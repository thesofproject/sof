/*
 * Copyright (c) 2016, Intel Corporation
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

/* Convert float frequency in Hz to Q16.16 fractional format */
#define TONE_FREQ(f) Q_CONVERT_FLOAT(f, 16)

/* Convert float gain to Q1.31 fractional format */
#define TONE_GAIN(v) Q_CONVERT_FLOAT(v, 31)

struct tone_state {
	int mute;
	int32_t a; /* Current amplitude Q1.31 */
	int32_t a_target; /* Target amplitude Q1.31 */
	int32_t ampl_coef; /* Amplitude multiplier Q2.30 */
	int32_t c; /* Coefficient 2*pi/Fs Q1.31 */
	int32_t f; /* Frequency Q16.16 */
	int32_t freq_coef; /* Frequency multiplier Q2.30 */
	int32_t fs; /* Sample rate in Hertz Q32.0 */
	int32_t ramp_step; /* Amplitude ramp step Q1.31 */
	int32_t w; /* Angle radians Q4.28 */
	int32_t w_step; /* Angle step Q4.28 */
	uint32_t block_count;
	uint32_t repeat_count;
	uint32_t repeats; /* Number of repeats for tone (sweep steps) */
	uint32_t sample_count;
	uint32_t samples_in_block; /* Samples in 125 us block */
	uint32_t tone_length; /* Active length in 125 us blocks */
	uint32_t tone_period; /* Active + idle time in 125 us blocks */
};
