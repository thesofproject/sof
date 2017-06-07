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

#ifndef EQ_FIR_H
#define EQ_FIR_H


/*
 * eq_fir_configuration data structure contains this information
 *     stream max channels
 *     number_of_responses_defined
 *         0=no respones, 1=one response defined, 2=two responses defined, etc.
 *     assign_response[STREAM_MAX_CHANNELS]
 *         -1 = not defined, 0 = use first response, 1 = use 2nd response, etc.
 *         E.g. {0, 0, 0, 0, -1, -1, -1, -1} would apply to channels 0-3 the
 *	   same first defined response and leave channels 4-7 unequalized.
 *     all_coefficients[]
 *         Repeated data { filter_length, input_shift, output_shift, h[] }
 *	   where vector h has filter_length number of coefficients.
 *	   Coefficients in h[] are in Q1.15 format. 16384 = 0.5. The shifts
 *	   are number of right shifts.
 */

#define NHEADER_EQ_FIR_BLOB 2 /* Header is two words plus assigns plus coef */

#define EQ_FIR_MAX_BLOB_SIZE 4096 /* Max size allowed for blob */

struct eq_fir_configuration {
	uint16_t stream_max_channels;
	uint16_t number_of_responses_defined;
	uint16_t assign_response[PLATFORM_MAX_CHANNELS];
	int16_t all_coefficients[];
};

struct eq_fir_update {
	uint16_t stream_max_channels;
	uint16_t assign_response[PLATFORM_MAX_CHANNELS];

};

#endif
