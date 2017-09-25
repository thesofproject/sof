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

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#ifdef MODULE_TEST
#include <stdio.h>
#endif

#include <reef/audio/format.h>
#include "fir.h"

#ifdef MODULE_TEST
#include <stdio.h>
#endif

/*
 * EQ FIR algorithm code
 */

void fir_reset(struct fir_state_32x16 *fir)
{
	fir->mute = 1;
	fir->rwi = 0;
	fir->length = 0;
	fir->delay_size = 0;
	fir->in_shift = 0;
	fir->out_shift = 0;
	fir->coef = NULL;
	/* There may need to know the beginning of dynamic allocation after
	 * reset so omitting setting also fir->delay to NULL.
	 */
}

int fir_init_coef(struct fir_state_32x16 *fir, int16_t config[])
{
	struct fir_coef_32x16 *setup;

	setup = (struct fir_coef_32x16 *) config;
	fir->mute = 0;
	fir->rwi = 0;
	fir->length = (int) setup->length;
	fir->in_shift = (int) setup->in_shift;
	fir->out_shift = (int) setup->out_shift;
	fir->coef = &setup->coef;
	fir->delay = NULL;
	fir->delay_size = 0;

	if ((fir->length > MAX_FIR_LENGTH) || (fir->length < 1))
		return -EINVAL;

	return fir->length;
}

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data)
{
	fir->delay = *data;
	fir->delay_size = fir->length;
	*data += fir->delay_size; /* Point to next delay line start */
}
