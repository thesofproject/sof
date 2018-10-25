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

#ifndef SRC_CONFIG_H
#define SRC_CONFIG_H

#include <config.h>

/* If next define is set to 1 the SRC is configured automatically. Setting
 * to zero temporarily is useful is for testing needs.
 */
#define SRC_AUTOARCH    1

/* Force manually some code variant when SRC_AUTODSP is set to zero. These
 * are useful in code debugging.
 */
#if SRC_AUTOARCH == 0
#define SRC_SHORT	0
#define SRC_GENERIC	1
#define SRC_HIFIEP	0
#define SRC_HIFI3	0
#endif

/* Select optimized code variant when xt-xcc compiler is used */
#if SRC_AUTOARCH == 1
#if defined __XCC__
#include <xtensa/config/core-isa.h>
#define SRC_GENERIC	0
#if XCHAL_HAVE_HIFI2EP == 1
#define SRC_SHORT	1  /* Select 16 bit coefficients to save RAM */
#define SRC_HIFIEP	1
#define SRC_HIFI3	0
#endif
#if XCHAL_HAVE_HIFI3 == 1
#define SRC_SHORT	0  /* Select 32 bit default quality coefficients */
#define SRC_HIFI3	1
#define SRC_HIFIEP	0
#endif
#else
/* GCC */
#if defined(CONFIG_HOST)
#define SRC_SHORT	0  /* Use high quality 32 bit filter coefficients */
#else
#define SRC_SHORT	1  /* Use 16 bit filter coefficients for speed */
#endif
#define SRC_GENERIC	1
#define SRC_HIFIEP	0
#define SRC_HIFI3	0
#endif
#endif

#endif
