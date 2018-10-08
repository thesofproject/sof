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

#ifndef FIR_CONFIG_H

/* Get platforms configuration */
#include <config.h>

/* If next defines are set to 1 the EQ is configured automatically. Setting
 * to zero temporarily is useful is for testing needs.
 * Setting EQ_FIR_AUTOARCH to 0 allows to manually set the code variant.
 */
#define FIR_AUTOARCH    1

/* Force manually some code variant when EQ_FIR_AUTODSP is set to zero. These
 * are useful in code debugging.
 */
#if FIR_AUTOARCH == 0
#define FIR_GENERIC	0
#define FIR_HIFIEP	0
#define FIR_HIFI3	1
#endif

/* Select optimized code variant when xt-xcc compiler is used */
#if FIR_AUTOARCH == 1
#if defined __XCC__
#include <xtensa/config/core-isa.h>
#define FIR_GENERIC	0
#if XCHAL_HAVE_HIFI2EP == 1
#define FIR_HIFIEP	1
#define FIR_HIFI3	0
#elif XCHAL_HAVE_HIFI3 == 1
#define FIR_HIFI3	1
#define FIR_HIFIEP	0
#else
#error "No HIFIEP or HIFI3 found. Cannot build FIR module."
#endif
#else
/* GCC */
#define FIR_GENERIC	1
#define FIR_HIFIEP	0
#define FIR_HIFI3	0
#endif
#endif

#define FIR_CONFIG_H

#endif
