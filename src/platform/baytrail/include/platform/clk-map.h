/*
 * Copyright (c) 2018, Intel Corporation
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
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __INCLUDE_CLOCK_MAP__
#define __INCLUDE_CLOCK_MAP__

#if defined CONFIG_BAYTRAIL
static const struct freq_table cpu_freq[] = {
	{25000000, 25000, 0x0},
	{25000000, 25000, 0x1},
	{50000000, 50000, 0x2},
	{50000000, 50000, 0x3}, /* default */
	{100000000, 100000, 0x4},
	{200000000, 200000, 0x5},
	{267000000, 267000, 0x6},
	{343000000, 343000, 0x7},
};
#elif defined CONFIG_CHERRYTRAIL
static const struct freq_table cpu_freq[] = {
	{19200000, 19200, 0x0},
	{19200000, 19200, 0x1},
	{38400000, 38400, 0x2},
	{50000000, 50000, 0x3}, /* default */
	{100000000, 100000, 0x4},
	{200000000, 200000, 0x5},
	{267000000, 267000, 0x6},
	{343000000, 343000, 0x7},
};
#endif

static const struct freq_table ssp_freq[] = {
	{19200000, 19200, PMC_SET_SSP_19M2}, /* default */
	{25000000, 25000, PMC_SET_SSP_25M},
};

#endif
