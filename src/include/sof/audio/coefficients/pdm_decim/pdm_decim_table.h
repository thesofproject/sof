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
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *
 */

/* PDM decimation FIR filters */

#include "pdm_decim_fir.h"

#if CONFIG_CAVS_DMIC_FIR_DECIMATE_BY_2
#include "pdm_decim_int32_02_4288_5100_010_095.h"
#include "pdm_decim_int32_02_4375_5100_010_095.h"
#endif
#if CONFIG_CAVS_DMIC_FIR_DECIMATE_BY_3
#include "pdm_decim_int32_03_3850_5100_010_095.h"
#include "pdm_decim_int32_03_4375_5100_010_095.h"
#endif
#if CONFIG_CAVS_DMIC_FIR_DECIMATE_BY_4
#include "pdm_decim_int32_04_4375_5100_010_095.h"
#endif
#if CONFIG_CAVS_DMIC_FIR_DECIMATE_BY_5
#include "pdm_decim_int32_05_4331_5100_010_095.h"
#endif
#if CONFIG_CAVS_DMIC_FIR_DECIMATE_BY_6
#include "pdm_decim_int32_06_4156_5100_010_095.h"
#endif
#if CONFIG_CAVS_DMIC_FIR_DECIMATE_BY_8
#include "pdm_decim_int32_08_4156_5380_010_090.h"
#endif

/* Note: Higher spec filter must be before lower spec filter
 * if there are multiple filters for a decimation factor. The naming
 * scheme of coefficients set is:
 * <type>_<decim factor>_<rel passband>_<rel stopband>_<ripple>_<attenuation>
 */
struct pdm_decim *fir_list[] = {
#if CONFIG_CAVS_DMIC_FIR_DECIMATE_BY_2
	&pdm_decim_int32_02_4375_5100_010_095,
	&pdm_decim_int32_02_4288_5100_010_095,
#endif
#if CONFIG_CAVS_DMIC_FIR_DECIMATE_BY_3
	&pdm_decim_int32_03_4375_5100_010_095,
	&pdm_decim_int32_03_3850_5100_010_095,
#endif
#if CONFIG_CAVS_DMIC_FIR_DECIMATE_BY_4
	&pdm_decim_int32_04_4375_5100_010_095,
#endif
#if CONFIG_CAVS_DMIC_FIR_DECIMATE_BY_5
	&pdm_decim_int32_05_4331_5100_010_095,
#endif
#if CONFIG_CAVS_DMIC_FIR_DECIMATE_BY_6
	&pdm_decim_int32_06_4156_5100_010_095,
#endif
#if CONFIG_CAVS_DMIC_FIR_DECIMATE_BY_8
	&pdm_decim_int32_08_4156_5380_010_090,
#endif
	NULL, /* This marks the end of coefficients */
};
