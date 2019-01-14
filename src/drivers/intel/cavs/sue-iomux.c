/*
 * Copyright (c) 2019, Intel Corporation
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
 * Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <sof/io.h>
#include <sof/iomux.h>

#include <platform/memory.h>

#define IOMUX_PIN_UNCONFIGURED 0

int iomux_configure(struct iomux *iomux, const struct iomux_pin_config *cfg)
{
	if (iomux->pin_state[cfg->bit] != IOMUX_PIN_UNCONFIGURED)
		return -EBUSY;

	io_reg_update_bits(iomux->base, cfg->mask << cfg->bit,
			   cfg->fn << cfg->bit);

	iomux->pin_state[cfg->bit] = cfg->fn + 1;

	return 0;
}

struct iomux *iomux_get(unsigned int id)
{
	return id >= n_iomux ? NULL : iomux_data + id;
}

int iomux_probe(struct iomux *iomux)
{
	unsigned int i;

	for (i = 0; i < n_iomux; i++)
		if (iomux_data + i == iomux)
			return 0;

	return -ENODEV;
}
