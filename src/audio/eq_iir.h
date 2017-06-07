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

#ifndef EQ_IIR_H
#define EQ_IIR_H



/* eq_iir_configuration
 *     uint32_t platform max channels
 *     uint32_t number_of_responses_defined
 *         0=no responses, 1=one response defined, 2=two responses defined, etc.
 *     uint32_t assign_response[PLATFORM_MAX_CHANNELS]
 *         -1 = not defined, 0 = use first response, 1 = use 2nd response, etc.
 *         E.g. {0, 0, 0, 0, -1, -1, -1, -1} would apply to channels 0-3 the
 *         same first defined response and leave channels 4-7 unequalized.
 *     all_coefficients[]
 *         <1st EQ>
 *         uint32_t num_biquads
 *         uint32_t num_biquads_in_series
 *         <1st biquad>
 *         int32_t coef_a2       Q2.30 format
 *         int32_t coef_a1       Q2.30 format
 *         int32_t coef_b2       Q2.30 format
 *         int32_t coef_b1       Q2.30 format
 *         int32_t coef_b0       Q2.30 format
 *         int32_t output_shift  number of shifts right, shift left is negative
 *         int32_t output_gain   Q2.14 format
 *         <2nd biquad>
 *         ...
 *         <2nd EQ>
 *
 *         Note: A flat response biquad can be made with a section set to
 *         b0 = 1.0, gain = 1.0, and other parameters set to 0
 *         {0, 0, 0, 0, 1073741824, 0, 16484}
 */

#define EQ_IIR_MAX_BLOB_SIZE 1024

#define NHEADER_EQ_IIR_BLOB 2 /* Blob is two words plus asssigns plus coef */

struct eq_iir_configuration {
	int32_t stream_max_channels;
	int32_t number_of_responses_defined;
	int32_t assign_response[PLATFORM_MAX_CHANNELS];
	int32_t all_coefficients[];
};

struct eq_iir_update {
	int32_t stream_max_channels;
	int32_t assign_response[PLATFORM_MAX_CHANNELS];
};

#endif
